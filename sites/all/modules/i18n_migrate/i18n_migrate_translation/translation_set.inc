<?php

class I18nTranslationSetMigration extends Migration {

  protected $sourceConnection;
  protected $type;
  protected $sourceOptions;

  public function __construct(array $arguments) {
    parent::__construct($arguments);

    if (!empty($arguments['source_connection'])) {
      $this->sourceConnection = $arguments['source_connection'];
    }

    if (!empty($arguments['source_bundle'])) {
      $this->sourceBundle = $arguments['source_bundle'];
    }

    if (!empty($arguments['type'])) {
      $this->type = $arguments['type'];
    }

    $this->sourceOptions = array();
    if (!empty($arguments['source_options'])) {
      $this->sourceOptions = $arguments['source_options'];
    }

    $this->source = new MigrateSourceSQL($this->query(), array(), NULL, $this->sourceOptions);
    $this->destination = new I18nMigrateDestinationTranslationSet($this->type);
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'tsid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'Source translation set ID',
          'alias' => 'ts',
        ),
      ),
      I18nMigrateDestinationTranslationSet::getKeySchema()
    );

    $this->addSimpleMappings(array('title', 'type', 'master_id', 'status', 'created', 'changed'));
    $mapping = $this->addFieldMapping('bundle');
    if (!empty($arguments['destination_bundle'])) {
      $mapping->defaultValue($arguments['destination_bundle']);
    }

  }



  /**
   * The base source query for this migration.
   *
   * @return SelectQueryInterface
   */
  protected function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
      ->select('i18n_translation_set', 'ts');
    $query->leftJoin('taxonomy_term_data', 'td', 'ts.tsid = td.i18n_tsid');

    $query->fields('ts', array('tsid', 'title', 'type', 'bundle', 'master_id', 'status', 'created', 'changed'))
          ->condition('ts.type', $this->type, '=');

    if (is_array($this->sourceBundle)) {
      $query->condition('ts.bundle', $this->sourceBundle, 'IN');
    } else {
      $query->condition('ts.bundle', $this->sourceBundle, '=');
    }

    $query->isNotNull('td.i18n_tsid');
    $query->distinct(TRUE);

    return $query;
  }
}