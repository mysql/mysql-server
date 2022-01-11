--echo #
--echo # WL#13469: secure storage for sensitive system variables
--echo #

# Data directory location
--let CURRENT_DATADIR = `SELECT @@datadir`
--let CURRENT_PERSISTED_FILE = `SELECT CONCAT('$CURRENT_DATADIR', '/mysqld-auto.cnf')`

--error 0,1
--remove_file $CURRENT_PERSISTED_FILE

# Pre-8.0.28 format persisted variables file
--let PRE8028_PERSISTED_VARIABLES_FILE = `SELECT CONCAT('$MYSQL_TEST_DIR', '/std_data/pre8028_mysqld-auto.cnf')`

--echo # Copy Pre-8.0.28 format persisted variables file to data directory
--copy_file $PRE8028_PERSISTED_VARIABLES_FILE $CURRENT_PERSISTED_FILE

--echo # Restart the server
--source include/restart_mysqld_no_echo.inc

--echo # Verify that persisted variable file was read properly: Should show 5 entries
SELECT * FROM performance_schema.persisted_variables ORDER BY variable_name ASC;

--echo # Check the actual values
SELECT @@global.partial_revokes;
SELECT @@global.auto_increment_increment;
SELECT @@global.net_buffer_length;
SELECT @@global.back_log;

--echo # Modify persisted variables
SET PERSIST_ONLY partial_revokes=OFF;
SET PERSIST_ONLY auto_increment_increment=30;
SET PERSIST_ONLY net_buffer_length=8192;
SET PERSIST_ONLY back_log=200;
SET PERSIST_ONLY performance_schema_error_size=10000;

--echo # Restart the server
--source include/restart_mysqld_no_echo.inc

--echo # Verify that persisted variable file was read properly: Should show 5 entries
SELECT * FROM performance_schema.persisted_variables ORDER BY variable_name ASC;

--echo # Check the actual values
SELECT @@global.partial_revokes;
SELECT @@global.auto_increment_increment;
SELECT @@global.net_buffer_length;
SELECT @@global.back_log;

RESET PERSIST;
--echo # Remove old format persisted variables file
--remove_file $CURRENT_PERSISTED_FILE

--echo # Cleanup: Restart with default options.
--source include/force_restart.inc
