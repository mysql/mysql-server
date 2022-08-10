--echo # Prepare for creating Encrypted Table
--let $CLONE_DATADIR = $MYSQL_TMP_DIR/data_new
--let $MYSQLD_DATADIR= `select @@datadir;`

--let $ENCRYPT_DATADIR = $MYSQL_TMP_DIR/data_encrypt
--let $ENCRYPT_ERROR_FILE = $MYSQL_TMP_DIR/boot_encrypt_log

--let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql
--let $START_PAGE_SIZE= `select @@innodb_page_size`

--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test DEFAULT CHARACTER SET latin1;
EOF
--mkdir $ENCRYPT_DATADIR

--echo # Create and start mysqld with keyring plugin.
--source include/shutdown_mysqld.inc

--replace_result $MYSQLD MYSQLD $BOOTSTRAP_SQL BOOTSTRAP_SQL $ENCRYPT_DATADIR ENCRYPT_DATADIR $START_PAGE_SIZE PAGE_SIZE $ENCRYPT_ERROR_FILE ENCRYPT_ERROR_FILE
--let NEW_CMD = $MYSQLD --no-defaults $PLUGIN_DIR_OPT --innodb_dedicated_server=OFF --initialize-insecure --init-file=$BOOTSTRAP_SQL --innodb_redo_log_capacity=10M --innodb_page_size=$START_PAGE_SIZE --datadir=$ENCRYPT_DATADIR > $ENCRYPT_ERROR_FILE  2>&1

--exec $NEW_CMD

--let $restart_parameters=restart: $PLUGIN_DIR_OPT --datadir=$ENCRYPT_DATADIR
--source include/start_mysqld_no_echo.inc

--echo ## Install plugin
--replace_result $CLONE_PLUGIN CLONE_PLUGIN
--eval INSTALL PLUGIN clone SONAME '$CLONE_PLUGIN'

## Create test schema
--source create_schema.inc

if(!$page_compression) {
--echo # In connection default - starting clone
# Validate that redo and undo encryption can be set when clone in progress. */
SET DEBUG_SYNC = 'clone_file_copy SIGNAL set_encryption WAIT_FOR resume_clone';
--source clone_command_send.inc

connect (con1,localhost,root,,);
--echo # In connection con1 - Set redo and undo encryption
SET DEBUG_SYNC = 'now WAIT_FOR set_encryption';

SHOW VARIABLES LIKE 'innodb_redo_log_encrypt';
SET GLOBAL innodb_redo_log_encrypt = ON;
SHOW VARIABLES LIKE 'innodb_redo_log_encrypt';

SHOW VARIABLES LIKE 'innodb_undo_log_encrypt';
SET GLOBAL innodb_undo_log_encrypt = ON;
SHOW VARIABLES LIKE 'innodb_undo_log_encrypt';

SET DEBUG_SYNC = 'now SIGNAL resume_clone';

connection default;
--echo # In connection default - finishing Clone
--reap
disconnect con1;
--force-rmdir $CLONE_DATADIR
SET GLOBAL innodb_redo_log_encrypt = OFF;
SET GLOBAL innodb_undo_log_encrypt = OFF;
}

# Modify the schema and add compressed table
DROP TABLE t1;
DROP TABLE t2;

if(!$page_compression) {
# Create table with encryption enabled */
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
ROW_FORMAT = COMPRESSED ENCRYPTION = "Y";

# Create table in encrypted general tablespace
CREATE TABLESPACE tbs1 ADD DATAFILE 'tbs1_data1.ibd' ENCRYPTION="Y";
CREATE TABLE t2(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
ENCRYPTION="Y" TABLESPACE = tbs1;

SET GLOBAL innodb_redo_log_encrypt = ON;
SHOW VARIABLES LIKE 'innodb_redo_log_encrypt';

SET GLOBAL innodb_undo_log_encrypt = ON;
SHOW VARIABLES LIKE 'innodb_undo_log_encrypt';
}

if($page_compression) {
# Create table with encryption and compression enabled.
# Disable warning for platforms not supporting punch hole.
--disable_warnings
CREATE TABLE t1(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
COMPRESSION = "LZ4"  ENCRYPTION = "Y";
--enable_warnings

# Create table in encrypted general tablespace
CREATE TABLESPACE tbs1 ADD DATAFILE 'tbs1_data1.ibd' ENCRYPTION="Y";

# Create table with ZLIB compression.
# Disable warning for platforms not supporting punch hole.
--disable_warnings
CREATE TABLE t2(col1 INT PRIMARY KEY, col2 int, col3 varchar(64), col4 BLOB)
COMPRESSION = "ZLIB";
--enable_warnings
}

## Execute Clone while concurrent DMLs are in progress

# Insert 200 rows
call execute_dml(0, 0, 200, 200, 100, 0);

# Check base rows
SHOW CREATE TABLE t1;

SELECT count(*) from t1;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;

SHOW CREATE TABLE t2;
SELECT count(*) from t2;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;

## Test-1: Clone with no redo log to copy.
SET GLOBAL innodb_buf_flush_list_now = 1;
--source clone_command.inc

--echo # Restart cloned database
--let $restart_parameters=restart: $PLUGIN_DIR_OPT --datadir=$CLONE_DATADIR
--source include/restart_mysqld_no_echo.inc

# Check table in cloned database
SHOW CREATE TABLE t1;
SELECT count(*) from t1;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;

SHOW CREATE TABLE t2;
SELECT count(*) from t2;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;

# Execute procedure to delete all rows and insert
call execute_dml(3, 0, 1, 1, 1, 0);
call execute_dml(0, 0, 200, 200, 100, 0);

SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;

SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;

--let $restart_parameters=restart: $PLUGIN_DIR_OPT --datadir=$ENCRYPT_DATADIR
--source include/restart_mysqld_no_echo.inc
--force-rmdir $CLONE_DATADIR

## Test-2: Clone with dirty pages and redo log
SET GLOBAL innodb_redo_log_encrypt = ON;
SET GLOBAL innodb_undo_log_encrypt = ON;

SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;

--echo # In connection default - Cloning database
SET DEBUG_SYNC = 'clone_file_copy SIGNAL start_dml1 WAIT_FOR resume_clone1';
SET DEBUG_SYNC = 'clone_page_copy SIGNAL start_dml2 WAIT_FOR resume_clone2';
--source clone_command_send.inc

--echo # In connection con1 - Running Update Random [0 - 200 Key Range]
connect (con1,localhost,root,,);
SET DEBUG_SYNC = 'now WAIT_FOR start_dml1';
START TRANSACTION;
CALL execute_dml(1, 0, 200, 500, 100, 1);
COMMIT;
--echo # Flush all dirty buffers
SET GLOBAL innodb_buf_flush_list_now = 1;
SET DEBUG_SYNC = 'now SIGNAL resume_clone1';

SET DEBUG_SYNC = 'now WAIT_FOR start_dml2';
START TRANSACTION;
CALL execute_dml(1, 0, 200, 500, 100, 1);
COMMIT;
SET DEBUG_SYNC = 'now SIGNAL resume_clone2';

connection default;
--echo # In connection default - Cloning database
--reap

disconnect con1;
--source include/wait_until_count_sessions.inc

--echo # Restart cloned database
--let $restart_parameters=restart: $PLUGIN_DIR_OPT --datadir=$CLONE_DATADIR
--source include/restart_mysqld_no_echo.inc

# Check table in cloned database
SHOW CREATE TABLE t1;
SELECT count(*) from t1;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;

SHOW CREATE TABLE t2;
SELECT count(*) from t2;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
SELECT col1, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;

# Execute procedure to delete all rows and insert
call execute_dml(3, 0, 1, 1, 1, 0);
call execute_dml(0, 0, 200, 200, 100, 0);

SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 LIMIT 10;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t1 ORDER BY col1 DESC LIMIT 10;

SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 LIMIT 10;
SELECT col1, col2, col3, SUBSTRING(col4, 1000, 32) FROM t2 ORDER BY col1 DESC LIMIT 10;

--let $restart_parameters=restart: $PLUGIN_DIR_OPT --datadir=$ENCRYPT_DATADIR
--source include/restart_mysqld_no_echo.inc
--force-rmdir $CLONE_DATADIR

# Cleanup
--source drop_schema.inc
DROP TABLESPACE tbs1;
UNINSTALL PLUGIN clone;
SET DEBUG_SYNC = 'RESET';

# Restart default database
--let restart_parameters="restart: $PLUGIN_DIR_OPT"
--source include/restart_mysqld_no_echo.inc

# Temporary file Cleanup
--force-rmdir $ENCRYPT_DATADIR
--remove_file $ENCRYPT_ERROR_FILE
--remove_file $BOOTSTRAP_SQL
