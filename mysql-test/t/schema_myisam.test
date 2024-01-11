#
# WL#6378: New data dictionary.
#
# Replace usage of 'check_db_dir_existence()' by
# 'dd::schema_exists()'. Error handling will be
# slightly different in some situations. Below,
# six test cases check the behavior.

--source include/force_myisam_default.inc
--source include/have_myisam.inc

--disable_query_log
CALL mtr.add_suppression("Failed to find tablespace");
CALL mtr.add_suppression("Ignoring tablespace");
CALL mtr.add_suppression("Cannot rename");
CALL mtr.add_suppression("Cannot calculate");
CALL mtr.add_suppression("Cannot open datafile");
CALL mtr.add_suppression("The error means the system cannot find");
CALL mtr.add_suppression("File ./s/t_innodb.ibd");
CALL mtr.add_suppression("Operating system error number");
--enable_query_log

# 1. Create schema, remove directory, then try CREATE TABLE.

# Schema directory path.
--let $MYSQL_DATA_DIR= `select @@datadir`
--let $SCHEMA_DIR= $MYSQL_DATA_DIR/s

# Create and remove schema directory.
CREATE SCHEMA s;
--rmdir $SCHEMA_DIR

# CREATE TABLE for MyISAM fails.
--replace_result $MYSQL_DATA_DIR MYSQL_DATA_DIR/
--error 1,1
CREATE TABLE s.t (pk INTEGER PRIMARY KEY);

# Re-create the directory, then DROP will work.
--mkdir $SCHEMA_DIR
DROP SCHEMA s;


# 2. Create schema, create tables, remove directory, then try ALTER TABLE.

# Schema directory path.
--let $MYSQL_DATA_DIR= `select @@datadir`
--let $SCHEMA_DIR= $MYSQL_DATA_DIR/s

# CREATE SCHEMA.
CREATE SCHEMA s;

# CREATE TABLE for MyISAM.
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);

# Shut server down.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

# Remove schema directory and files.
--remove_files_wildcard $SCHEMA_DIR *
--rmdir $SCHEMA_DIR

# Restart the server.
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

# ALTER TABLE for MyISAM fails.
--error ER_FILE_NOT_FOUND
ALTER TABLE s.t_myisam ADD COLUMN c INTEGER;

# Re-create the directory, then DROP will work.
--mkdir $SCHEMA_DIR
DROP SCHEMA s;

# 3. Create schema, create tables, remove directory, then try SHOW CREATE TABLE.

# Schema directory path.
--let $MYSQL_DATA_DIR= `select @@datadir`
--let $SCHEMA_DIR= $MYSQL_DATA_DIR/s

# CREATE SCHEMA.
CREATE SCHEMA s;

# CREATE TABLE for MyISAM.
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);

# Shut server down.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

# Remove schema directory and files.
--remove_files_wildcard $SCHEMA_DIR *
--rmdir $SCHEMA_DIR

# Restart the server.
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

# SHOW CREATE TABLE for MyISAM fails.
--error ER_FILE_NOT_FOUND
SHOW CREATE TABLE s.t_myisam;

# Re-create the directory, then DROP will work.
--mkdir $SCHEMA_DIR
DROP SCHEMA s;

# 4. Create schema, create tables, remove directory, then try DROP TABLE.

# Schema directory path.
--let $MYSQL_DATA_DIR= `select @@datadir`
--let $SCHEMA_DIR= $MYSQL_DATA_DIR/s

# CREATE SCHEMA.
CREATE SCHEMA s;

# CREATE TABLE for MyISAM.
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);

# Shut server down.
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--shutdown_server
--source include/wait_until_disconnected.inc

# Remove schema directory and files.
--remove_files_wildcard $SCHEMA_DIR *
--rmdir $SCHEMA_DIR

# Restart the server.
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--source include/wait_until_connected_again.inc

# DROP TABLE for MyISAM fails.
--error ER_ENGINE_CANT_DROP_MISSING_TABLE
DROP TABLE s.t_myisam;

# Re-create the directory, then DROP will work.
--mkdir $SCHEMA_DIR
DROP SCHEMA s;

# End of tests for WL#6378: New data dictionary.

