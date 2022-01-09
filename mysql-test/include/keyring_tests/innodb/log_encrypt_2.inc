# InnoDB transparent tablespace data encryption
# This test case will test basic encryption support features.

#Suppress following messages from myslqd log
--disable_query_log
call mtr.add_suppression("\\[ERROR\\] .* Can't set redo log files to be encrypted.");
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
--enable_query_log

# Restart with redo log encryption enabled
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

SHOW VARIABLES LIKE "%innodb_redo_log_encrypt%";

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

let $innodb_file_per_table = `SELECT @@innodb_file_per_table`;

SET GLOBAL innodb_file_per_table = 1;
SELECT @@innodb_file_per_table;

# Create a table with encryption
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;

SHOW CREATE TABLE t1;
INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;
INSERT INTO t1 select * from t1;

SELECT * FROM t1 LIMIT 10;

# Restart to confirm the encryption info can be retrieved properly.
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

SHOW VARIABLES LIKE "%innodb_redo_log_encrypt%";

SELECT * FROM t1 LIMIT 10;

# Key rotation.
ALTER INSTANCE ROTATE INNODB MASTER KEY;

DROP TABLE t1;

# Crash/recovery test.
CREATE TABLE t1(c1 INT, c2 char(20)) ENCRYPTION="Y" ENGINE = InnoDB;

INSERT INTO t1 VALUES(0, "aaaaa");
INSERT INTO t1 VALUES(1, "bbbbb");
INSERT INTO t1 VALUES(2, "ccccc");
INSERT INTO t1 VALUES(3, "ddddd");
INSERT INTO t1 VALUES(4, "eeeee");
INSERT INTO t1 VALUES(5, "fffff");
INSERT INTO t1 VALUES(6, "ggggg");
INSERT INTO t1 VALUES(7, "hhhhh");
INSERT INTO t1 VALUES(8, "iiiii");
INSERT INTO t1 VALUES(9, "jjjjj");

# Restart to confirm the encryption info can be retrieved properly.
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

SHOW VARIABLES LIKE "%innodb_redo_log_encrypt%";

SELECT * FROM t1 LIMIT 10;
DROP TABLE t1;

# Restart to confirm the encryption info can be retrieved properly.
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON --general-log --log-output=FILE --general_log_file=$MYSQL_TMP_DIR/keyring_query_log
--source include/start_mysqld_no_echo.inc

SHOW VARIABLES LIKE "%innodb_redo_log_encrypt%";

#
# Check no effect of block_encryption_mode = 'aes-256-cbc' variable on table encryption
SET block_encryption_mode = 'aes-256-cbc';
# Test encryption .
--disable_warnings
DROP DATABASE IF EXISTS tde_db;
CREATE DATABASE tde_db;
CREATE TABLE tde_db.t1(c1 INT PRIMARY KEY, c2 char(50)) ENCRYPTION = 'Y' ENGINE = InnoDB;
--enable_warnings
#
INSERT INTO tde_db.t1 VALUES(0, 'abc');
INSERT INTO tde_db.t1 VALUES(1, 'xyz');
INSERT INTO tde_db.t1 VALUES(2, null);
INSERT INTO tde_db.t1 VALUES(3, null);
SELECT * FROM tde_db.t1 LIMIT 10;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT * FROM tde_db.t1 LIMIT 10;
--echo # Mysqldump output
--exec $MYSQL_DUMP --compact --skip-comments --databases tde_db
--echo # Redirecting mysqlpump output to MYSQL_TMP_DIR/mysqlpump_encrypt.sql
--exec $MYSQL_PUMP --default-parallelism=1 --databases tde_db > $MYSQL_TMP_DIR/mysqlpump_encrypt.sql
DROP DATABASE tde_db;

--let SEARCH_FILE=$MYSQL_TMP_DIR/keyring_query_log
let SEARCH_PATTERN= ALTER INSTANCE ROTATE INNODB MASTER KEY;
--source include/search_pattern.inc

--echo # Loading tables from mysqlpump_encrypt.sql
--exec $MYSQL --skip-comments < $MYSQL_TMP_DIR/mysqlpump_encrypt.sql
SELECT * FROM tde_db.t1 LIMIT 10;
INSERT INTO tde_db.t1 VALUES(4, null);
SELECT * FROM tde_db.t1 LIMIT 10;
DROP DATABASE tde_db;

--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

# Cleanup
--remove_file $MYSQL_TMP_DIR/keyring_query_log
--remove_file $MYSQL_TMP_DIR/mysqlpump_encrypt.sql
eval SET GLOBAL innodb_file_per_table=$innodb_file_per_table;
