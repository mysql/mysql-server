# InnoDB transparent encryption on redo log.
# This test case will test basic encryption support features.

#Suppress following messages from myslqd log
--disable_query_log
call mtr.add_suppression("Error while loading keyring content. The keyring might be malformed");
call mtr.add_suppression("\\[Warning\\] .* Plugin mysqlx reported.*");
call mtr.add_suppression("\\[Warning\\] .* Ignoring tablespace .* because it could not be opened");
call mtr.add_suppression("\\[ERROR\\] .* Failed to find tablespace for table `\.\.*`\.`\.\.*` in the cache.");
call mtr.add_suppression("\\[ERROR\\] .* Can't set redo log files to be encrypted.");
--enable_query_log

# Restart with redo log encryption enabled
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

let $old_innodb_redo_log_encrypt = `SELECT @@innodb_redo_log_encrypt`;

CREATE DATABASE tde_db;
USE tde_db;

# Flush logs
#  (closes and reopens any flushable logs for installed storage engines.
#  This causes InnoDB to flush its logs to disk. )
# Restart with redo log encryption enabled
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc

SELECT @@global.innodb_redo_log_encrypt ;
SET GLOBAL innodb_redo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t1 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t1 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
CREATE TABLE tde_db.t2 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t2 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t2;
SET GLOBAL innodb_redo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t3 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
INSERT INTO t3 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t4;
FLUSH LOGS;
# Restart with redo log encryption enabled
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc


SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;
DROP TABLE tde_db.t1,tde_db.t2,tde_db.t3,tde_db.t4;

# innodb_redo_log_encrypt=ON/OFF  with transaction
SELECT @@global.innodb_redo_log_encrypt ;
CREATE TABLE tde_db.t1 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
CREATE TABLE tde_db.t2 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
START TRANSACTION;
SET GLOBAL innodb_redo_log_encrypt = 1;
INSERT INTO t1 (a, b) VALUES (1, REPEAT('a', 6*512*512));
INSERT INTO t2 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
ROLLBACK;
START TRANSACTION;
INSERT INTO t1 (a, b) VALUES (2, REPEAT('a', 6*512*512));
INSERT INTO t2 (a, b) VALUES (2, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
COMMIT;
CREATE TABLE tde_db.t3 (a BIGINT PRIMARY KEY, b LONGBLOB) ENGINE=InnoDB;
CREATE TABLE tde_db.t4 (a BIGINT PRIMARY KEY, b LONGBLOB)
                        ENCRYPTION='Y' ENGINE=InnoDB;
START TRANSACTION;
SET GLOBAL innodb_redo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
INSERT INTO t3 (a, b) VALUES (1, REPEAT('a', 6*512*512));
INSERT INTO t4 (a, b) VALUES (1, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;
ROLLBACK;
START TRANSACTION;
INSERT INTO t3 (a, b) VALUES (2, REPEAT('a', 6*512*512));
INSERT INTO t4 (a, b) VALUES (2, REPEAT('a', 6*512*512));
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;
COMMIT;
# Restart with redo log encryption enabled
--source include/kill_mysqld.inc
--let $restart_parameters= restart: $PLUGIN_DIR_OPT --innodb_redo_log_encrypt=ON
--source include/start_mysqld_no_echo.inc


SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;

# Rotate master key when innodb_redo_log_encrypt=ON/OFF
SET GLOBAL innodb_redo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;
SET GLOBAL innodb_redo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
ALTER INSTANCE ROTATE INNODB MASTER KEY;
SELECT a,LEFT(b,10) FROM tde_db.t1;
SELECT a,LEFT(b,10) FROM tde_db.t2;
SELECT a,LEFT(b,10) FROM tde_db.t3;
SELECT a,LEFT(b,10) FROM tde_db.t4;

# Non priviledge user can not set innodb_redo_log_encrypt variable.
CREATE USER encryptnonprivuser@localhost IDENTIFIED BY 'noauth';
GRANT SELECT ON *.* to encryptnonprivuser@localhost;
FLUSH PRIVILEGES;
--echo # In connection 1 - with encryptnonprivuser
connect (con1,localhost,encryptnonprivuser,'noauth',);
SELECT @@global.innodb_redo_log_encrypt ;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL innodb_redo_log_encrypt = 0;
SELECT @@global.innodb_redo_log_encrypt ;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL innodb_undo_log_encrypt = 0;
SELECT @@global.innodb_undo_log_encrypt ;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL innodb_redo_log_encrypt = 1;
SELECT @@global.innodb_redo_log_encrypt ;
--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL innodb_undo_log_encrypt = 1;
SELECT @@global.innodb_undo_log_encrypt ;
disconnect con1;
--echo # In connection default
connection default;

DROP TABLE tde_db.t1,tde_db.t2,tde_db.t3,tde_db.t4;

# Cleanup
--disable_query_log
eval SET GLOBAL innodb_redo_log_encrypt=$old_innodb_redo_log_encrypt;
DROP USER encryptnonprivuser@localhost;
DROP DATABASE tde_db;
--enable_query_log
