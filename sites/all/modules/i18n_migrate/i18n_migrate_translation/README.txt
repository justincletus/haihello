Description
-----------
This module allows you to migrate translation sets, it provides translation_set
destination and I18nTranslationSetMigration migration class.


Instruction
-----------
In your migration you have available some new options:

- source_connection      - database connection you want to use for migration.
- source_bundle          - bundle of source entity that is using translation
                           set. It can be an array if you want for example
                           merge multiple vocabularies from source website on
                           destination website, then you can use an array here
                           to also merge translation sets across vocabularies.
- destination_bundle     - if you want to remap terms to the new vocabulary on
                           destination website, put here the name of
                           destination vocabulary.
- type                   - type of translatable content that is translateable,
                           it might be an entity type like taxonomy_term, but
                           doesn't need to (can be for example also menu_link).


Example migration:

  'translation_sets' => array(
    'class_name' => 'I18nTranslationSetMigration',
    'description' => t('Migration of translation sets of taxonomy from Drupal 7 website.'),
    'source_connection' => 'legacy',
    'source_bundle' => array('themes', 'musicians'),
    'type' => 'taxonomy_term',
    'destination_bundle' => 'tags',
    'source_options' => array('map_joinable' => FALSE),
    'group_name' => 'mywebsite',
  ),

