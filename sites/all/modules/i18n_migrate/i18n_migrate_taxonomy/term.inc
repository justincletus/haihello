<?php

class I18nDrupalTerm7Migration extends DrupalTerm7Migration {

  public function __construct($arguments) {

    parent::__construct($arguments);

    if ($arguments['tset_migration']) {

      $this->dependencies[] = $arguments['tset_migration'];

      $this->addSimpleMappings(array('language'));
      $this->addFieldMapping('i18n_tsid', 'i18n_tsid')
           ->sourceMigration($arguments['tset_migration'])
           ->defaultValue(0);
    }
  }

  protected function query() {
    $query = parent::query();
    $query->addField('td', 'i18n_tsid');
    $query->addField('td', 'language');
    return $query;
  }

}