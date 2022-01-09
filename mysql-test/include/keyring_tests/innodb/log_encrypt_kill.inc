# InnoDB transparent encryption on redo log and undo log
# This test case will test basic encryption support with 
# concuurent execution and

--source include/no_valgrind_without_big.inc
--source include/have_innodb_max_16k.inc

--disable_query_log
call mtr.add_suppression('Error in Log_event::read_log_event()');
--enable_query_log

let $old_innodb_file_per_table = `SELECT @@innodb_file_per_table`;
let $old_innodb_redo_log_encrypt = `SELECT @@innodb_redo_log_encrypt`;
let $START_PAGE_SIZE= `select @@innodb_page_size`;
let $LOG_SIZE= `select @@innodb_redo_log_capacity`;
let $MYSQLD_BASEDIR= `select @@basedir`;

let SEARCH_FILE= $MYSQL_TMP_DIR/encrypt_log_output.err;

# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc

# Test that the output appears.
let $restart_parameters = restart: $PLUGIN_DIR_OPT --no-console --log-error=$SEARCH_FILE;
--source include/restart_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt;
SET GLOBAL innodb_redo_log_encrypt = 1;
SET GLOBAL innodb_undo_log_encrypt = 1;

--let $wait_condition=SELECT @@global.innodb_redo_log_encrypt = 0
--source include/wait_condition.inc

SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

# Create Non encrypt table with innodb_redo_log_encrypt=ON
CREATE TABLE tne_1(c1 INT, c2 varchar(2000)) ENGINE = InnoDB;
INSERT INTO tne_1 VALUES (1,REPEAT('a',1990)),(2,REPEAT('b',1990)),(100,REPEAT('c',1990));
SELECT c1,LEFT(c2,10) FROM tne_1;
DROP TABLE tne_1;
--echo # Stop the MTR default DB server
--source include/shutdown_mysqld.inc

# Grep for message in server error log
let SEARCH_PATTERN=Can\'t set redo log files to be encrypted;
--source include/search_pattern.inc

# Create path for ibdata* & undo* files both DBs
--mkdir $MYSQL_TMP_DIR/innodb_undo_data_dir
--mkdir $MYSQL_TMP_DIR/datadir
# Set path for --datadir
let $MYSQLD_DATADIR_1 = $MYSQL_TMP_DIR/datadir/data;
# Set path for undo* files.
let $MYSQLD_UNDO_DATADIR = $MYSQL_TMP_DIR/innodb_undo_data_dir;
let BOOTSTRAP_SQL=$MYSQL_TMP_DIR/boot.sql;
--echo # create bootstrap file
write_file $BOOTSTRAP_SQL;
CREATE DATABASE test;
EOF

# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo # Prepare new datadir
let NEW_CMD = $MYSQLD --no-defaults --innodb_dedicated_server=OFF --initialize-insecure --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --basedir=$MYSQLD_BASEDIR --datadir=$MYSQLD_DATADIR_1 --init-file=$BOOTSTRAP_SQL --secure-file-priv="" $PLUGIN_DIR_OPT </dev/null>>$MYSQLTEST_VARDIR/tmp/bootstrap2.log 2>&1;

--echo # Run the bootstrap command with keyring
--exec $NEW_CMD

--echo # Starting server with keyring plugin
--let $restart_parameters=restart: $PLUGIN_DIR_OPT --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR_1
--source include/start_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt;
SET GLOBAL innodb_redo_log_encrypt = 1;
SELECT @@global.innodb_undo_log_encrypt;
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_undo_log_encrypt;

SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

SET GLOBAL innodb_redo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt;
SET GLOBAL innodb_undo_log_encrypt = 0;
SELECT @@global.innodb_undo_log_encrypt;

SELECT status_key, status_value FROM
performance_schema.keyring_component_status
WHERE status_key LIKE '%name%' OR status_key LIKE '%status%';

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
USE tde_db;
--enable_warnings

let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

# Create a table with encryption
CREATE TABLE tde_db.t_encrypt(c2 INT NOT NULL PRIMARY KEY,
      c3 LONGBLOB
      ) ENCRYPTION="Y"  ENGINE = InnoDB;

CREATE TABLE tde_db.t_non_encrypt(c2 INT NOT NULL PRIMARY KEY,
      c3 LONGBLOB
      ) ENGINE = InnoDB;


DELIMITER |;
CREATE PROCEDURE tde_db.populate_table_set_redo_encrypt(IN table_name VARCHAR(50))
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 2000) DO

                SET @sql_text = CONCAT('INSERT INTO ',table_name, '(c2,c3) VALUES (' , i,',' ,'CONCAT(REPEAT("a",6*512*512)))');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                set i = i + 1;
                IF i%10 = 0 THEN SET GLOBAL innodb_redo_log_encrypt = 1;
                END IF;
                IF i%20 = 0 THEN SET GLOBAL innodb_redo_log_encrypt = 0;
                END IF;
                IF i%15 = 0 THEN SET GLOBAL innodb_undo_log_encrypt = 1;
                END IF;
                IF i%30 = 0 THEN SET GLOBAL innodb_undo_log_encrypt = 0;
                END IF;

        end while;
end|
CREATE PROCEDURE tde_db.update_table(IN table_name VARCHAR(50))
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 2000) DO
                SET @sql_text = CONCAT('UPDATE ',table_name,  ' SET c2 = c2 * -1 ORDER BY RAND() LIMIT 2');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.delete_table(IN table_name VARCHAR(50))
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 2000) DO
                SET @sql_text = CONCAT('DELETE FROM  ',table_name,  '  ORDER BY RAND() LIMIT 2');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.transaction_table(IN table_name VARCHAR(50))
begin
        declare i int default 1;
        declare iflag int default -1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        SET i = 3000;
        START TRANSACTION;
        while (i <= 9000) DO
                SET @sql_text = CONCAT('INSERT INTO ',table_name, '(c2,c3) VALUES (' , i,',' ,'CONCAT(REPEAT("a",6*512*512)))');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                SET @sql_text = CONCAT('UPDATE ',table_name,  ' SET c2 = c2 * -1 ORDER BY RAND() LIMIT 2');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                set i = i + 1;
                IF i%10 = 0 THEN SET GLOBAL innodb_redo_log_encrypt = 1;
                END IF;
                IF i%20 = 0 THEN SET GLOBAL innodb_redo_log_encrypt = 0;
                END IF;
                IF i%15 = 0 THEN SET GLOBAL innodb_undo_log_encrypt = 1;
                END IF;
                IF i%30 = 0 THEN SET GLOBAL innodb_undo_log_encrypt = 0;
                END IF;

                IF i%10 = 0 THEN
                   SET @sql_text = CONCAT('DELETE FROM  ',table_name,  '  ORDER BY RAND() LIMIT 2');
                   PREPARE stmt FROM @sql_text;
                   EXECUTE stmt;
                   START TRANSACTION;
                   SET iflag = -1 * iflag;
                END IF;
                IF i%9 = 0 THEN
                   IF iflag < 0 THEN
                      COMMIT;
                   ELSE
                      ROLLBACK;
                   END IF;
                END IF;
        end while;
end|
CREATE PROCEDURE tde_db.create_table_rotate_key()
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 2000) DO

                IF i%10 = 0 THEN
                SET @sql_text = CONCAT('CREATE TABLE ',CONCAT('tde_db.t_non_encrypt_',encrypt,'_',i),' (c1 INT) ' ,' ENGINE=InnoDB');
                ELSE
                SET @sql_text = CONCAT('CREATE TABLE ',CONCAT('tde_db.t_encrypt_',encrypt,'_',i),' (c1 INT) ENCRYPTION="Y"' ,' ENGINE=InnoDB');
                END IF;
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                DEALLOCATE PREPARE stmt;
                ALTER INSTANCE ROTATE INNODB MASTER KEY;
                set i = i + 1;
        end while;
end|
CREATE PROCEDURE tde_db.query_table(IN table_name VARCHAR(50))
begin
        declare i int default 1;
        declare has_error int default 0;
        DECLARE CONTINUE HANDLER FOR 1062 SET has_error = 1;
        while (i <= 2000) DO
                SET @sql_text = CONCAT('SELECT * FROM ',table_name,  '  ORDER BY RAND() LIMIT 2');
                PREPARE stmt FROM @sql_text;
                EXECUTE stmt;
                set i = i + 1;
        end while;
end|
DELIMITER ;|

--echo # In connection con1 - Running insert with redo_log_encrypt variable
connect (con1,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_encrypt');

--echo # In connection con2 - Running insert on non encrypt table
connect (con2,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_non_encrypt');

--connection default
--sleep 3
--echo # Starting server with keyring
let $restart_parameters = restart: $PLUGIN_DIR_OPT --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR_1;
--sleep 3
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

--disconnect con1
--disconnect con2
--connection default

--replace_column 1 #
SELECT COUNT(*)>0  FROM tde_db.t_encrypt;
--replace_column 1 #
SELECT COUNT(*)>0  FROM tde_db.t_non_encrypt;

DELETE FROM tde_db.t_encrypt;
DELETE FROM tde_db.t_non_encrypt;

--echo # In connection con1 - Running insert with redo_log_encrypt variable
connect (con1,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_encrypt');

--echo # In connection con2 - Running insert on non encrypt table
connect (con2,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_non_encrypt');

--echo # In connection con3 - Running update on encrypt
connect (con3,localhost,root,,);
send call tde_db.table_update('tde_db.t_encrypt');
--echo # In connection con4 - Running update non encrypt
connect (con4,localhost,root,,);
send call tde_db.table_update('tde_db.t_non_encrypt');
--echo # In connection con5 - Running delete on encrypt
connect (con5,localhost,root,,);
send call tde_db.table_delete('tde_db.t_encrypt');
--echo # In connection con6 - Running delete on non encrypt
connect (con6,localhost,root,,);
send call tde_db.table_delete('tde_db.t_non_encrypt');
--echo # In connection con7 - Running transaction on  encrypt
connect (con7,localhost,root,,);
send call tde_db.transaction_table('tde_db.t_encrypt');
--echo # In connection con8 - Running transaction on non encrypt
connect (con8,localhost,root,,);
send call tde_db.transaction_table('tde_db.t_non_encrypt');
--echo # In connection con9 - Running create encrypt and non encrypt table with rotate key
connect (con9,localhost,root,,);
send call tde_db.create_table_rotate_key();
--echo # In connection con10 - Running query on encrypt table
connect (con10,localhost,root,,);
send call tde_db.query_table('tde_db.t_encrypt');
--echo # In connection con11 - Running query on non encrypt table
connect (con11,localhost,root,,);
send call tde_db.query_table('tde_db.t_non_encrypt');

--connection default
--sleep 20
--echo # Starting server with keyring
let $restart_parameters = restart: $PLUGIN_DIR_OPT --innodb_redo_log_capacity=$LOG_SIZE --innodb_page_size=$START_PAGE_SIZE --innodb_undo_directory=$MYSQLD_UNDO_DATADIR --datadir=$MYSQLD_DATADIR_1;
--let $_server_id= `SELECT @@server_id`
--let $_expect_file_name= $MYSQLTEST_VARDIR/tmp/mysqld.$_server_id.expect
--exec echo "$restart_parameters" > $_expect_file_name
--shutdown_server 0
--source include/wait_until_disconnected.inc
--enable_reconnect
--source include/wait_until_connected_again.inc
--disable_reconnect

--disconnect con1
--disconnect con2
--disconnect con3
--disconnect con4
--disconnect con5
--disconnect con6
--disconnect con7
--disconnect con8
--disconnect con9
--disconnect con10
--disconnect con11

--connection default
--replace_column 1 #
SELECT COUNT(*) > 1 FROM tde_db.t_encrypt;
--replace_column 1 #
SELECT COUNT(*) > 1 FROM tde_db.t_non_encrypt;

# Run some load insert retart
--echo # In connection con1 - Running insert with redo_log_encrypt variable
connect (con1,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_encrypt');

--echo # In connection con2 - Running insert on non encrypt table
connect (con2,localhost,root,,);
send call tde_db.populate_table_set_redo_encrypt('tde_db.t_non_encrypt');

--sleep 2
--disconnect con1
--disconnect con2
--connection default
DROP DATABASE tde_db;

# Take backup of global manifest file
--source include/keyring_tests/helper/binary_backup_manifest.inc
# restart the server with MTR default
--let $restart_parameters=
--source include/restart_mysqld.inc
# Restore global manifest file
--source include/keyring_tests/helper/binary_restore_manifest.inc

--echo #
--echo # Cleanup
--echo #
--remove_file $MYSQLTEST_VARDIR/tmp/bootstrap2.log
--remove_file $BOOTSTRAP_SQL
--remove_file $SEARCH_FILE
--force-rmdir $MYSQL_TMP_DIR/datadir
--force-rmdir $MYSQL_TMP_DIR/innodb_undo_data_dir

--disable_query_log
eval SET GLOBAL innodb_file_per_table=$old_innodb_file_per_table;
eval SET GLOBAL innodb_redo_log_encrypt=$old_innodb_redo_log_encrypt;
--enable_query_log
