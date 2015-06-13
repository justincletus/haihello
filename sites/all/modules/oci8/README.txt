OCI8 module

This module is used to connect to Oracle databases using Drupal's familiar
database functions and classes, such as oci8_db_query(),
oci8_db_query_range(), $query->execute(), $query->fetchAll(), etc.

This module wraps PHP's OCI8 functions (http://www.php.net/manual/en/book.oci8.php)
with Drupal's Database, DatabaseConnection, and DatabaseStatement classes, see
asu_oracle.database.inc for more information about classes defined by this module.

Install module as usual in /sites/all/modules. Before enabling, you must define
at lease one database connection in settings.php as described below.

<?php

/**
 * Database connection settings for the oci8 module. These settings
 * mimic Drupal's main $databases variable.
 */
$conf['oci8'] = array(
  'databases' => array(
    'default' => array(
      'default' => array(
        'database' => 'DATABASE_NAME',
        'username' => 'DATABASE_USER_NAME',
        'password' => 'DATABASE_PASSWORD',
        'hosts' => array('HOST_A', 'HOST_B'),
        'port' => 'PORT_NUMBER',
      ),
    ),
  ),
);

?>

TODO

1) Build out OCI8OracleSelectQuery class which will mimic Drupal's SelectQuery.
   This would include writing an oci8_db_select() function to create these
   objects.