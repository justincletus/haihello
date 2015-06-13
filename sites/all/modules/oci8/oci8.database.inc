<?php

/**
 * @file
 * The OCI8 database classes.
 */

/**
 * The classes used by this module both duplicate and piggyback on some of
 * Drupal's database abstraction layer classes.
 *
 * OCI8Database: duplicated from Database class because it uses final static
 * methods which were not able to be overridden through inheritance. The class
 * is mainly untouched other than the parseConnectionInfo() method which reads
 * from a the global $conf['oci8'] array rather than $databases set in the
 * site's settings.php file. $conf['oci8']['databases'] has the same array
 * format as $databases except the 'host' key is renamed 'hosts', which can be
 * an array of hostnames.
 *
 * OCI8DatabaseConnection: extends the DatabaseConnection class overridding
 * many of its methods. DatabaseConnection extends the PHP PDO class which is why
 * we use some of PDO's class contants, especially the fetch methods.
 *
 * OCI8DatabaseStatement: extends the DatabaseStatementPrefetch class and
 * overrides the execute() method. Since the parent class's data fetching methods
 * all use a local data store, this means that we do not need to override any
 * of those methods leaving the hard part already done.
 *
 * OCI8Exception: almost duplicates the PDOException class which the Drupal
 * database classes throw.
 *
 */

abstract class OCI8Database {

  /**
   * Flag to indicate a query call should simply return NULL.
   *
   * This is used for queries that have no reasonable return value anyway, such
   * as INSERT statements to a table without a serial primary key.
   */
  const RETURN_NULL = 0;

  /**
   * Flag to indicate a query call should return the prepared statement.
   */
  const RETURN_STATEMENT = 1;

  /**
   * Flag to indicate a query call should return the number of affected rows.
   */
  const RETURN_AFFECTED = 2;

  /**
   * Flag to indicate a query call should return the "last insert id".
   */
  const RETURN_INSERT_ID = 3;

  /**
   * An nested array of all active connections. It is keyed by database name
   * and target.
   *
   * @var array
   */
  static protected $connections = array();

  /**
   * A processed copy of the database connection information from settings.php.
   *
   * @var array
   */
  static protected $databaseInfo = NULL;

  /**
   * A list of key/target credentials to simply ignore.
   *
   * @var array
   */
  static protected $ignoreTargets = array();

  /**
   * The key of the currently active database connection.
   *
   * @var string
   */
  static protected $activeKey = 'default';

  /**
   * An array of active query log objects.
   *
   * Every connection has one and only one logger object for all targets and
   * logging keys.
   *
   * array(
   *   '$db_key' => DatabaseLog object.
   * );
   *
   * @var array
   */
  static protected $logs = array();

  /**
   * Starts logging a given logging key on the specified connection.
   *
   * @param $logging_key
   *   The logging key to log.
   * @param $key
   *   The database connection key for which we want to log.
   *
   * @return DatabaseLog
   *   The query log object. Note that the log object does support richer
   *   methods than the few exposed through the Database class, so in some
   *   cases it may be desirable to access it directly.
   *
   * @see DatabaseLog
   */
  final public static function startLog($logging_key, $key = 'default') {
    if (empty(self::$logs[$key])) {
      self::$logs[$key] = new DatabaseLog($key);

      // Every target already active for this connection key needs to have the
      // logging object associated with it.
      if (!empty(self::$connections[$key])) {
        foreach (self::$connections[$key] as $connection) {
          $connection->setLogger(self::$logs[$key]);
        }
      }
    }

    self::$logs[$key]->start($logging_key);
    return self::$logs[$key];
  }

  /**
   * Retrieves the queries logged on for given logging key.
   *
   * This method also ends logging for the specified key. To get the query log
   * to date without ending the logger request the logging object by starting
   * it again (which does nothing to an open log key) and call methods on it as
   * desired.
   *
   * @param $logging_key
   *   The logging key to log.
   * @param $key
   *   The database connection key for which we want to log.
   *
   * @return array
   *   The query log for the specified logging key and connection.
   *
   * @see DatabaseLog
   */
  final public static function getLog($logging_key, $key = 'default') {
    if (empty(self::$logs[$key])) {
      return NULL;
    }
    $queries = self::$logs[$key]->get($logging_key);
    self::$logs[$key]->end($logging_key);
    return $queries;
  }

  /**
   * Gets the connection object for the specified database key and target.
   *
   * @param $target
   *   The database target name.
   * @param $key
   *   The database connection key. Defaults to NULL which means the active key.
   *
   * @return DatabaseConnection
   *   The corresponding connection object.
   */
  final public static function getConnection($target = 'default', $key = NULL) {
    if (!isset($key)) {
      // By default, we want the active connection, set in setActiveConnection.
      $key = self::$activeKey;
    }
    // If the requested target does not exist, or if it is ignored, we fall back
    // to the default target. The target is typically either "default" or
    // "slave", indicating to use a slave SQL server if one is available. If
    // it's not available, then the default/master server is the correct server
    // to use.
    if (!empty(self::$ignoreTargets[$key][$target]) || !isset(self::$databaseInfo[$key][$target])) {
      $target = 'default';
    }

    if (!isset(self::$connections[$key][$target])) {
      // If necessary, a new connection is opened.
      self::$connections[$key][$target] = self::openConnection($key, $target);
    }
    return self::$connections[$key][$target];
  }

  /**
   * Determines if there is an active connection.
   *
   * Note that this method will return FALSE if no connection has been
   * established yet, even if one could be.
   *
   * @return
   *   TRUE if there is at least one database connection established, FALSE
   *   otherwise.
   */
  final public static function isActiveConnection() {
    return !empty(self::$activeKey) && !empty(self::$connections) && !empty(self::$connections[self::$activeKey]);
  }

  /**
   * Sets the active connection to the specified key.
   *
   * @return
   *   The previous database connection key.
   */
  final public static function setActiveConnection($key = 'default') {
    if (empty(self::$databaseInfo)) {
      self::parseConnectionInfo();
    }

    if (!empty(self::$databaseInfo[$key])) {
      $old_key = self::$activeKey;
      self::$activeKey = $key;
      return $old_key;
    }
  }

  /**
   * Process the configuration file for database information.
   */
  final public static function parseConnectionInfo() {
    global $conf;

    $database_info = is_array($conf['oci8']['databases']) ? $conf['oci8']['databases'] : array();
    foreach ($database_info as $index => $info) {
      foreach ($database_info[$index] as $target => $value) {
        // Parse the prefix information.
        if (!isset($database_info[$index][$target]['prefix'])) {
          // Default to an empty prefix.
          $database_info[$index][$target]['prefix'] = array(
            'default' => '',
          );
        }
        elseif (!is_array($database_info[$index][$target]['prefix'])) {
          // Transform the flat form into an array form.
          $database_info[$index][$target]['prefix'] = array(
            'default' => $database_info[$index][$target]['prefix'],
          );
        }
      }
    }

    if (!is_array(self::$databaseInfo)) {
      self::$databaseInfo = $database_info;
    }

    // Merge the new $database_info into the existing.
    // array_merge_recursive() cannot be used, as it would make multiple
    // database, user, and password keys in the same database array.
    else {
      foreach ($database_info as $database_key => $database_values) {
        foreach ($database_values as $target => $target_values) {
          self::$databaseInfo[$database_key][$target] = $target_values;
        }
      }
    }
  }
  
  /**
   * Adds database connection information for a given key/target.
   *
   * This method allows the addition of new connection credentials at runtime.
   * Under normal circumstances the preferred way to specify database
   * credentials is via settings.php. However, this method allows them to be
   * added at arbitrary times, such as during unit tests, when connecting to
   * admin-defined third party databases, etc.
   *
   * If the given key/target pair already exists, this method will be ignored.
   *
   * @param $key
   *   The database key.
   * @param $target
   *   The database target name.
   * @param $info
   *   The database connection information, as it would be defined in
   *   settings.php. Note that the structure of this array will depend on the
   *   database driver it is connecting to.
   */
  public static function addConnectionInfo($key, $target, $info) {
    if (empty(self::$databaseInfo[$key][$target])) {
      self::$databaseInfo[$key][$target] = $info;
    }
  }

  /**
   * Gets information on the specified database connection.
   *
   * @param $connection
   *   The connection key for which we want information.
   */
  final public static function getConnectionInfo($key = 'default') {
    if (empty(self::$databaseInfo)) {
      self::parseConnectionInfo();
    }

    if (!empty(self::$databaseInfo[$key])) {
      return self::$databaseInfo[$key];
    }
  }

  /**
   * Rename a connection and its corresponding connection information.
   *
   * @param $old_key
   *   The old connection key.
   * @param $new_key
   *   The new connection key.
   * @return
   *   TRUE in case of success, FALSE otherwise.
   */
  final public static function renameConnection($old_key, $new_key) {
    if (empty(self::$databaseInfo)) {
      self::parseConnectionInfo();
    }

    if (!empty(self::$databaseInfo[$old_key]) && empty(self::$databaseInfo[$new_key])) {
      // Migrate the database connection information.
      self::$databaseInfo[$new_key] = self::$databaseInfo[$old_key];
      unset(self::$databaseInfo[$old_key]);

      // Migrate over the DatabaseConnection object if it exists.
      if (isset(self::$connections[$old_key])) {
        self::$connections[$new_key] = self::$connections[$old_key];
        unset(self::$connections[$old_key]);
      }

      return TRUE;
    }
    else {
      return FALSE;
    }
  }

  /**
   * Remove a connection and its corresponding connection information.
   *
   * @param $key
   *   The connection key.
   * @return
   *   TRUE in case of success, FALSE otherwise.
   */
  final public static function removeConnection($key) {
    if (isset(self::$databaseInfo[$key])) {
      self::closeConnection(NULL, $key);
      unset(self::$databaseInfo[$key]);
      return TRUE;
    }
    else {
      return FALSE;
    }
  }

  /**
   * Opens a connection to the server specified by the given key and target.
   *
   * @param $key
   *   The database connection key, as specified in settings.php. The default is
   *   "default".
   * @param $target
   *   The database target to open.
   *
   * @throws OCI8DatabaseConnectionNotDefinedException
   */
  final protected static function openConnection($key, $target) {
    if (empty(self::$databaseInfo)) {
      self::parseConnectionInfo();
    }
    
    // If the requested database does not exist then it is an unrecoverable
    // error.
    if (!isset(self::$databaseInfo[$key])) {
      throw new OCI8DatabaseConnectionNotDefinedException('The specified database connection is not defined: ' . $key);
    }
    
    // We cannot rely on the registry yet, because the registry requires an
    // open database connection.
    $new_connection = new OCI8DatabaseConnection(self::$databaseInfo[$key][$target]);
    $new_connection->setTarget($target);
    $new_connection->setKey($key);

    // If we have any active logging objects for this connection key, we need
    // to associate them with the connection we just opened.
    if (!empty(self::$logs[$key])) {
      $new_connection->setLogger(self::$logs[$key]);
    }

    return $new_connection;
  }

  /**
   * Closes a connection to the server specified by the given key and target.
   *
   * @param $target
   *   The database target name.  Defaults to NULL meaning that all target
   *   connections will be closed.
   * @param $key
   *   The database connection key. Defaults to NULL which means the active key.
   */
  public static function closeConnection($target = NULL, $key = NULL) {
    // Gets the active connection by default.
    if (!isset($key)) {
      $key = self::$activeKey;
    }
    // To close a connection, it needs to be set to NULL and removed from the
    // static variable. In all cases, closeConnection() might be called for a
    // connection that was not opened yet, in which case the key is not defined
    // yet and we just ensure that the connection key is undefined.
    if (isset($target)) {
      if (isset(self::$connections[$key][$target])) {
        self::$connections[$key][$target]->destroy();
        self::$connections[$key][$target] = NULL;
      }
      unset(self::$connections[$key][$target]);
    }
    else {
      if (isset(self::$connections[$key])) {
        foreach (self::$connections[$key] as $target => $connection) {
          self::$connections[$key][$target]->destroy();
          self::$connections[$key][$target] = NULL;
        }
      }
      unset(self::$connections[$key]);
    }
  }

  /**
   * Instructs the system to temporarily ignore a given key/target.
   *
   * At times we need to temporarily disable slave queries. To do so, call this
   * method with the database key and the target to disable. That database key
   * will then always fall back to 'default' for that key, even if it's defined.
   *
   * @param $key
   *   The database connection key.
   * @param $target
   *   The target of the specified key to ignore.
   */
  public static function ignoreTarget($key, $target) {
    self::$ignoreTargets[$key][$target] = TRUE;
  }

  /**
   * Load a file for the database that might hold a class.
   *
   * @param $driver
   *   The name of the driver.
   * @param array $files
   *   The name of the files the driver specific class can be.
   */
  public static function loadDriverFile($driver, array $files = array()) {
    static $base_path;

    if (empty($base_path)) {
      $base_path = dirname(realpath(__FILE__));
    }

    $driver_base_path = "$base_path/$driver";
    foreach ($files as $file) {
      // Load the base file first so that classes extending base classes will
      // have the base class loaded.
      foreach (array("$base_path/$file", "$driver_base_path/$file") as $filename) {
        // The OS caches file_exists() and PHP caches require_once(), so
        // we'll let both of those take care of performance here.
        if (file_exists($filename)) {
          require_once $filename;
        }
      }
    }
  }
}

class OCI8DatabaseConnection extends DatabaseConnection {
  
  private $connectionString = NULL;
  protected $connection = NULL;
  
  public function __construct(array $connection_options = array()) {
    // This driver defaults to transaction support, except if explicitly passed FALSE.
    $this->transactionSupport = !isset($connection_options['transactions']) || ($connection_options['transactions'] !== FALSE);

    // MySQL never supports transactional DDL.
    $this->transactionalDDLSupport = FALSE;

    $this->connectionOptions = $connection_options;

    // Create the connection string. Hosts can be a string but check just to
    // make sure.
    $hosts = '';
    if (!is_array($connection_options['hosts'])) {
      $connection_options['host'] = array($connection_options['hosts']);
    }
    foreach ($connection_options['hosts'] as $host) {
      $hosts .= "(ADDRESS = (PROTOCOL = TCP)(HOST = {$host})(PORT = {$connection_options['port']}))\n";
    }
    $this->connectionString = "(DESCRIPTION = 
      {$hosts}
      (LOAD_BALANCE = yes)
      (CONNECT_DATA = 
        (SERVICE_NAME = {$connection_options['database']})
      ))";
    
    // Create the OCI connection.
    $this->connection = oci_connect($connection_options['username'], $connection_options['password'], $this->connectionString);
  }
  
  /**
   * Make sure the connection actually closes.
   */
  public function __destruct() {
    oci_close($this->connection);
  }
  
  public function getConnection() {
    return $this->connection;
  }
  
  public function databaseType() {
    return 'oracle';
  }
  
  public function driver() {
    return 'oci';
  }
  
  public function mapConditionOperator($operator) {
  
  }
  
  public function nextId($existing_id = 0) {
    
  }
  
  public function prepareQuery($query) {
    return $this->prepare($this->prefixTables($query));
  }
  
  public function prepare($query, $options = array()) {
    return new OCI8DatabaseStatement($this, $query, $options);
  }

  public function query($query, array $args = array(), $options = array()) {

    // Use default values if not already set.
    $options += $this->defaultOptions();
  
    try {
      // We allow either a pre-bound statement object or a literal string.
      // In either case, we want to end up with an executed statement object,
      // which we pass to PDOStatement::execute.
      if ($query instanceof DatabaseStatementInterface) {
        $stmt = $query;
        $stmt->execute(NULL, $options);
      }
      else {
        $this->expandArguments($query, $args);
        $stmt = $this->prepareQuery($query);
        $stmt->execute($args, $options);
      }
        
      // Depending on the type of query we may need to return a different value.
      // See OCI8DatabaseConnection::defaultOptions() for a description of each
      // value.
      switch ($options['return']) {
        case OCI8Database::RETURN_STATEMENT:
          return $stmt;
        case OCI8Database::RETURN_AFFECTED:
          return $stmt->rowCount();
        case OCI8Database::RETURN_INSERT_ID:
          return $this->lastInsertId();
        case OCI8Database::RETURN_NULL:
          return;
        default:
          throw new OCI8Exception('Invalid return directive: ' . $options['return']);
      }
      
      return ($stmt) ? $stmt : NULL;
    }
    catch (OCI8Exception $e) {
      if ($options['throw_exception']) {
        // Add additional debug information.
        if ($query instanceof DatabaseStatementInterface) {
          $e->query_string = $stmt->getQueryString();
        }
        else {
          $e->query_string = $query;
        }
        $e->args = $args;
        throw $e;
      }
      return NULL;
    }
  }
  
  /**
   * The method used to query a range of records was provided at
   * http://www.oracle.com/technetwork/issue-archive/2006/06-sep/o56asktom-086197.html
   */
  public function queryRange($query, $from, $count, array $args = array(), array $options = array()) {
    $args[':oci8_MIN_ROW_TO_FETCH'] = (int) $from;
    $args[':oci8_MAX_ROW_TO_FETCH'] = (int) $from + (int) $count - 1;
    $query = "SELECT * 
        FROM ( SELECT
        oci8_QUERY_RANGE.*, ROWNUM oci8_ROWNUM
          FROM ( $query ) oci8_QUERY_RANGE
          WHERE ROWNUM <= :oci8_MAX_ROW_TO_FETCH ) 
      WHERE oci8_ROWNUM >= :oci8_MIN_ROW_TO_FETCH";
    return $this->query($query, $args, $options);
  }
  
  /**
   * Temporary tables are most likely never going to be allowed on Oracle
   * databases. If this is not the case then we can develop this out when needed.
   */
  public function queryTemporary($query, array $args = array(), array $options = array()) {
    return NULL;
  }
  
  public function errorInfo ($resource = NULL) {
    return ($resource) ? oci_error($resource) : oci_error();
  }
}

/**
 * Extending the DatabaseStatementPrefetch class in order to allow for more
 * control of how the data is pulled. The statement is executed and then stored
 * in a local data store $this->data. All data extraction after execution is
 * done with this local data store which frees up the connection to the database.
 * Another reason for prefetching the entire result set is due to how
 * oci_fetch_all() works. It specifically says that the function "can be called
 * only once for each query executed with oci_execute()", which would mean
 * having to keep track of when the statement ever reached the end of the
 * resultset and having to re-execute.
 */
class OCI8DatabaseStatement extends DatabaseStatementPrefetch implements Iterator, DatabaseStatementInterface {

  /**
   * Constructor needs to be overridden because $connection needs to be of type
   * OCI8DatabaseConnection instead of the normal DatabaseConnection.
   */
  public function __construct(OCI8DatabaseConnection $connection, $query, array $driver_options = array()) {
    $this->dbh = $connection;
    $this->queryString = $query;
    $this->driverOptions = $driver_options;
  }

  public function execute($args = array(), $options = array()) {
    global $user;
    
    if (isset($options['fetch'])) {
      if (is_string($options['fetch'])) {
        // Default to an object. Note: db fields will be added to the object
        // before the constructor is run. If you need to assign fields after
        // the constructor is run, see http://drupal.org/node/315092.
        $this->setFetchMode(PDO::FETCH_CLASS, $options['fetch']);
      }
      else {
        $this->setFetchMode($options['fetch']);
      }
    }

    $logger = $this->dbh->getLogger();
    if (!empty($logger)) {
      $query_start = microtime(TRUE);
    }
    
    // Prepare the query.
    $statement = $this->getStatement($this->queryString, $args);
    if (!$statement) {
      $this->throwOCI8Exception($statement);
    }
    
    // Bind variables to the statement. Add the OCI8_DRUPAL_USER_TOKEN to
    // $args first in case it is in the query.
    
    $args[OCI8_DRUPAL_USER_TOKEN] = $user->name;
    foreach ($args as $key => $value) {
      // oci_bind_by_name($statement, $key, $value) does not work
      // because it binds each placeholder to the same location: $value
      // instead use the actual location of the data: $args[$key]
      if (strpos($this->queryString, $key)) {
        $bind = oci_bind_by_name($statement, $key, $args[$key]);
        if (!$bind) {
          $this->throwOCI8Exception($statement);
          break;
        }
      }
    }
    
    $return = oci_execute($statement);
    if (!$return) {
      $this->throwOCI8Exception($statement);
    }
    
    // Fetch all the data from the reply, in order to release any lock
    // as soon as possible.
    $this->rowCount = oci_num_rows($statement);
    oci_fetch_all($statement, $this->data, 0, -1, OCI_FETCHSTATEMENT_BY_ROW + OCI_ASSOC);
    
    // Destroy the statement as soon as possible. See
    // DatabaseConnection_sqlite::PDOPrepare() for explanation.
    oci_free_statement($statement);
    
    $this->resultRowCount = count($this->data);

    if ($this->resultRowCount) {
      $this->columnNames = array_keys($this->data[0]);
    }
    else {
      $this->columnNames = array();
    }
    
    if (!empty($logger)) {
      $query_end = microtime(TRUE);
      $logger->log($this, $args, $query_end - $query_start);
    }
    
    // Initialize the first row in $this->currentRow.
    $this->next();
    
    return $return;
  }
  
  /**
   * Overrides parent fetchAll() to match take into consideration supplied $fetch_style.
   */
  public function fetchAll($fetch_style = NULL, $fetch_column = NULL, $constructor_args = NULL) {
    $this->fetchStyle = isset($fetch_style) ? $fetch_style : $this->defaultFetchStyle;
    $this->fetchOptions = $this->defaultFetchOptions;
    if (isset($fetch_column)) {
      $this->fetchOptions['column'] = $fetch_column;
    }
    if (isset($constructor_args)) {
      $this->fetchOptions['constructor_args'] = $constructor_args;
    }
  
    $result = array();
    // Traverse the array as PHP would have done.
    while (isset($this->currentRow)) {
      // Grab the row in the format specified above.
      $result[] = $this->current();
      $this->next();
    }
  
    // Reset the fetch parameters to the value stored using setFetchMode().
    $this->fetchStyle = $this->defaultFetchStyle;
    $this->fetchOptions = $this->defaultFetchOptions;
    return $result;
  }
  
  protected function getStatement($query, &$args = array()) {
    return oci_parse($this->dbh->getConnection(), $query);
  }
  
  /**
   * Throw an OCI8Exception based on the last OCI error.
   */
  protected function throwOCI8Exception($resource = NULL) {
    $error_info = $this->dbh->errorInfo($resource);
    // We rebuild a message formatted in the same way as PDO.
    $exception = new OCI8Exception("SQLSTATE[" . $error_info['code'] . "]: General error " . $error_info['message'] . ": " . $error_info['sqltext']);
    $exception->errorInfo = $error_info;
    throw $exception;
  }
}

/**
 * General exception for OCI8* classes and mimics PDOException.
 */
class OCI8Exception extends RuntimeException {
  public $errorInfo;
  protected $code;
}

/**
 * Exception thrown if an undefined database connection is requested.
 */
class OCI8DatabaseConnectionNotDefinedException extends Exception {
}
