<?php

/**
 * @file
 * Support for node destinations.
 */

// TODO:
// Make sure this works with updates, explicit destination keys

/**
 * Destination class implementing migration into nodes.
 */
class I18nMigrateDestinationTranslationSet extends MigrateDestinationEntity {
  protected $bypassDestIdCheck = FALSE;

  static public function getKeySchema() {
    return array(
      'tsid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'description' => 'ID of destination translation set',
      ),
    );
  }

  /**
   * Basic initialization
   *
   * @param string $bundle
   *  A.k.a. the content type (page, article, etc.) of the node.
   * @param array $options
   *  Options applied to nodes.
   */
  public function __construct($bundle, array $options = array()) {
    parent::__construct('translation_set', $bundle, $options);
  }

  /**
   * Returns a list of fields available to be mapped for the translation_set (bundle)
   *
   * @param Migration $migration
   *  Optionally, the migration containing this destination.
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields($migration = NULL) {
    $fields = array();
    // First the core (translation_set table) properties
    $fields['tsid'] = t('Translation set: ID');
    $fields['title'] = t('Translation set: Title');
    $fields['bundle'] = t('Translation set: Bundle');
    $fields['master_id'] = t('Translation set: Master ID');
    $fields['status'] = t('Translation set: Status');
    $fields['created'] = t('Translation set: Created timestamp');
    $fields['changed'] = t('Translation set: Modified timestamp');


    // Then add in anything provided by handlers
    $fields += migrate_handler_invoke_all('Entity', 'fields', $this->entityType, $this->bundle, $migration);
    $fields += migrate_handler_invoke_all('TranslationSet', 'fields', $this->entityType, $this->bundle, $migration);

    return $fields;
  }

  /**
   * Delete a batch of translation sets at once.
   *
   * @param $tsids
   *  Array of node IDs to be deleted.
   */
  public function bulkRollback(array $tsids) {
    migrate_instrument_start('translation_set_delete_multiple');
    $this->prepareRollback($tsids);
    foreach($tsids as $tsid) {
      $set = i18n_translation_set_load($tsid);
      // Translation set may be deleted at term deletion, so delete it only if
      // it exists.
      if ($set) {
        $set->delete(TRUE);
      }

    }
    $this->completeRollback($tsids);
    migrate_instrument_stop('translation_set_delete_multiple');
  }

  /**
   * Import a single node.
   *
   * @param $translation_set
   *  Node object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   * @return array
   *  Array of key fields (nid only in this case) of the node that was saved if
   *  successful. FALSE on failure.
   * @throws
   */
  public function import(stdClass $translation_set, stdClass $row) {
    // Updating previously-migrated content?
    $migration = Migration::currentMigration();
    if (isset($row->migrate_map_destid1) && !$this->bypassDestIdCheck) {
      if (isset($translation_set->tsid)) {
        if ($translation_set->tsid != $row->migrate_map_destid1) {
          throw new MigrateException(t("Incoming tsid !tsid and map destination nid !destid1 don't match",
            array('!tsid' => $translation_set->tsid, '!destid1' => $row->migrate_map_destid1)));
        }
      }
      else {
        $translation_set->tsid = $row->migrate_map_destid1;
      }
    }

    if ($migration->getSystemOfRecord() == Migration::DESTINATION) {
      if (!isset($translation_set->tsid)) {
        throw new MigrateException(t('System-of-record is DESTINATION, but no destination tsid provided'));
      }
      $old_translation_set = i18n_translation_set_load($translation_set->tsid);
      if (empty($old_translation_set)) {
        throw new MigrateException(t('System-of-record is DESTINATION, but translation_set !tsid does not exist',
                                   array('!tsid' => $translation_set->tsid)));
      }
      if (!isset($translation_set->created)) {
        $translation_set->created = $old_translation_set->created;
      }
      if (!isset($translation_set->status)) {
        $translation_set->status = $old_translation_set->status;
      }
      if (!isset($translation_set->title)) {
        $translation_set->title = $old_translation_set->title;
      }
    }

    if (!isset($translation_set->type)) {
      // Default the type to our designated destination bundle (by doing this
      // conditionally, we permit some flexibility in terms of implementing
      // migrations which can affect more than one type).
      $translation_set->type = $this->bundle;
    }

    // Set some required properties.

    if ($migration->getSystemOfRecord() == Migration::SOURCE) {

      if (isset($translation_set->created)) {
        $created = $translation_set->created = MigrationBase::timestamp($translation_set->created);
      }
      else {
        $translation_set->created = REQUEST_TIME;
      }
      if (isset($translation_set->changed)) {
        $changed = MigrationBase::timestamp($translation_set->changed);
      }
    }

    // Invoke migration prepare handlers
    $this->prepare($translation_set, $row);

    // Trying to update an existing node
    if ($migration->getSystemOfRecord() == Migration::DESTINATION) {
      // Incoming data overrides existing data, so only copy non-existent fields
      foreach ($old_translation_set as $field => $value) {
        // An explicit NULL in the source data means to wipe to old value (i.e.,
        // don't copy it over from $old_node)
        if (property_exists($translation_set, $field) && $translation_set->$field === NULL) {
          // Ignore this field
        }
        elseif (!isset($translation_set->$field)) {
          $translation_set->$field = $old_translation_set->$field;
        }
      }
    }

    if (isset($translation_set->tsid)) {
      $updating = TRUE;
    }
    else {
      $updating = FALSE;
    }

    migrate_instrument_start('translation_set_save');
    $translation_set = i18n_translation_set_build($translation_set->type, get_object_vars($translation_set));
    $translation_set->insert(FALSE);
    migrate_instrument_stop('translation_set_save');

    if (isset($translation_set->tsid)) {
      if ($updating) {
        $this->numUpdated++;
      }
      else {
        $this->numCreated++;
      }

      // We need to set "created" and "changed" properities once again because,
      // translation set object overrides them on "insert" unfortunately.

      if (isset($changed)) {
        db_update('i18n_translation_set')
          ->fields(array('changed' => $changed))
          ->condition('tsid', $translation_set->tsid)
          ->execute();
        $translation_set->changed = $changed;
      }

      if (isset($created)) {
        db_update('i18n_translation_set')
          ->fields(array('created' => $created))
          ->condition('tsid', $translation_set->tsid)
          ->execute();
        $translation_set->created = $created;
      }

      $return = array($translation_set->tsid);
    }
    else {
      $return = FALSE;
    }

    $this->complete($translation_set, $row);
    return $return;
  }
}
