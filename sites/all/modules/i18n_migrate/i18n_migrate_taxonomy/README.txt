Description
-----------

This module provides you only a migration class I18nDrupalTerm7Migration.



Instruction
-----------
You will firstly need to migrate translation sets with the help of
i18n_migrate_translation, then you have available migration class
I18nDrupalTerm7Migration. This class extends DrupalTerm7Migration class from
migrate_d2d module, so it has the same options plus some of its own.

Distinct options:

- tset_migration - the name of migration of translation sets that contain
                   translation sets for those terms.


Example migration:

  'themes' => array(
    'class_name' => 'I18nDrupalTerm7Migration',
    'description' => t('Migration of "themes" terms from Drupal 7 website.'),
    'source_vocabulary' => 'themes',
    'destination_vocabulary' => 'tags',
    'tset_migration' => 'translation_sets',
    'source_connection' => 'legacy',
    'source_version' => 7,
    'group_name' => 'euroradio',
  ),
