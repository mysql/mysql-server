###############################################################################
# MTR Test: innodb_native_foreign_keys system variable
#
# This test validates the behavior of the new 'innodb_native_foreign_keys'
# system variable, which enables users to switch between SQL and InnoDB foreign
# key handling for InnoDB tables. It covers default values, scope checks,
# mutability, functionality with both ON and OFF settings, deprecation
# warnings, and cleanup procedures.
###############################################################################

--echo # FR 8) Add innodb_native_foreign_keys system variable for switching FK handling.

###############################################################################
# Test Case: Initial startup, verify default value of innodb_native_foreign_keys
###############################################################################
SHOW VARIABLES LIKE 'innodb_native_foreign_keys';

--echo # Test scope is Global

###############################################################################
# Test Case: Validate variable scope and that it cannot be set at session level
###############################################################################
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SELECT @@SESSION.innodb_native_foreign_keys;
SELECT @@GLOBAL.innodb_native_foreign_keys;

###############################################################################
# Test Case: Attempt to change system variable (should error; static only)
###############################################################################
--echo # Test Mutable Type is static
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
SET GLOBAL innodb_native_foreign_keys = OFF;

###############################################################################
# Test Case: When variable is OFF, SQL foreign key handling must apply for InnoDB
###############################################################################
--echo # FR 8.1) innodb_native_foreign_keys=OFF = SQL FK handling for InnoDB
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE t2 (f1 INT, f2 INT,
  FOREIGN KEY (f1) REFERENCES t1 (f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1,1), (2,2), (3,3);
INSERT INTO t2 VALUES (1,1), (2,2), (3,3);
DELETE FROM t1 WHERE f1=1;
--echo # rows_deleted should tally child table records (SQL FK handling)
SELECT table_name, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
DROP TABLE t1, t2;

###############################################################################
# Test Case: Restart with innodb_native_foreign_keys=ON (enables native handling)
###############################################################################
--echo # Request server restart with innodb_native_foreign_keys system variable ON
--exec echo "restart: --innodb_native_foreign_keys=ON" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect

###############################################################################
# Test Case: Check for deprecation warning in server error log
###############################################################################
--echo # FR 8.4) Startup with innodb_native_foreign_keys should deprecate in log
--let $restart_parameters=restart: --innodb_native_foreign_keys=ON
--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The syntax '--innodb_native_foreign_keys' is deprecated and will be removed
--source include/search_pattern.inc

###############################################################################
# Test Case: Check innodb_native_foreign_keys value after restart
###############################################################################
--echo # Check innodb_native_foreign_keys after restart
SHOW VARIABLES LIKE 'innodb_native_foreign_keys';

###############################################################################
# Test Case: innodb_native_foreign_keys=ON - InnoDB native FK handling for InnoDB
###############################################################################
--echo # FR 8.2) innodb_native_foreign_keys=ON = native InnoDB FK handling
CREATE TABLE t1 (f1 INT PRIMARY KEY, f2 INT);
CREATE TABLE t2 (f1 INT, f2 INT,
  FOREIGN KEY (f1) REFERENCES t1 (f1) ON DELETE CASCADE ON UPDATE CASCADE);
INSERT INTO t1 VALUES (1,1), (2,2), (3,3);
INSERT INTO t2 VALUES (1,1), (2,2), (3,3);
DELETE FROM t1 WHERE f1=1;
--echo # rows_deleted should NOT tally child table records (native FK handling)
SELECT table_name, rows_deleted FROM sys.schema_table_statistics
  WHERE table_schema='test' ORDER BY table_name;
DROP TABLE t1, t2;

###############################################################################
# Test Case: Restart with innodb_native_foreign_keys set to default (OFF)
###############################################################################
--echo # restart with innodb_native_foreign_keys default value
--exec echo "restart: --innodb_native_foreign_keys=OFF" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart: --innodb_native_foreign_keys=OFF
--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc

###############################################################################
# Test Case: Confirm deprecation warning for system variable OFF value
###############################################################################
--let SEARCH_FILE=$MYSQLTEST_VARDIR/log/mysqld.1.err
--let SEARCH_PATTERN=The syntax '--innodb_native_foreign_keys' is deprecated and will be removed
--source include/search_pattern.inc

#WL 17024 - Activate triggers on referencing tables during foreign key CASCADE
###############################################################################
--echo # FR 2: Test new system variable enable_cascade_triggers
###############################################################################
CREATE TABLE logtable (id INT PRIMARY KEY AUTO_INCREMENT, tbl_name VARCHAR(16), operation VARCHAR(16), val int);
CREATE TABLE t1 (f1 INT PRIMARY KEY);
CREATE TABLE t2 (f1 INT UNIQUE REFERENCES t1(f1) ON DELETE CASCADE);
INSERT INTO t1 VALUES (1), (2), (3), (4), (5);
INSERT INTO t2 VALUES (1), (2), (3), (4), (5);
CREATE TRIGGER t2_ad AFTER DELETE ON t2
    FOR EACH ROW INSERT INTO logtable(tbl_name, operation, val)
    VALUES ('t2', 'DELETE', OLD.f1);

###############################################################################
--echo # FR 2.1: When innodb_native_foreign_keys = ON, enable_cascade_triggers
--echo # variable must not have any effect; foreign keys and cascades remain
--echo # handled inside InnoDB with no SQL-layer cascade-trigger firing
--echo # restart with innodb_native_foreign_keys = ON
###############################################################################
--exec echo "restart: --innodb_native_foreign_keys=ON" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart: --innodb_native_foreign_keys=ON
--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc
SET enable_cascade_triggers = OFF;
DELETE FROM t1 WHERE f1 = 1;
SELECT * FROM logtable;
DELETE FROM logtable;

SET enable_cascade_triggers = ON;
DELETE FROM t1 WHERE f1 = 2;
SELECT * FROM logtable;
DELETE FROM logtable;

###############################################################################
--echo # FR 2.2: When innodb_native_foreign_keys = OFF and
--echo # enable_cascade_triggers = ON, triggers on rows affected by SQL-layer
--echo # FK cascades must be fired.
--echo # restart with innodb_native_foreign_keys default value
###############################################################################
--exec echo "restart: --innodb_native_foreign_keys=OFF" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart: --innodb_native_foreign_keys=OFF
--source include/restart_mysqld.inc
--source include/wait_until_connected_again.inc
SET enable_cascade_triggers = ON;
DELETE FROM t1 WHERE f1 = 3;
SELECT * FROM logtable;
DELETE FROM logtable;

###############################################################################
--echo # FR 2.3: When innodb_native_foreign_keys = OFF and
--echo # enable_cascade_triggers = OFF, child table triggers must not fire
--echo # during cascade changes
###############################################################################
SET enable_cascade_triggers = OFF;
DELETE FROM t1 WHERE f1 = 4;
SELECT * FROM logtable;
DELETE FROM logtable;

DROP TABLE logtable, t1, t2;

###############################################################################
# Test Case: Cleanup: Restart without innodb_native_foreign_keys system variable
###############################################################################
--echo # cleanup: restart without innodb_native_foreign_keys variable
--exec echo "restart:" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--let $restart_parameters=restart:
--source include/restart_mysqld.inc

###############################################################################
# Test Case: Confirm variable state after cleanup restart
###############################################################################
--echo # After cleanup restart
SHOW VARIABLES LIKE 'innodb_native_foreign_keys';
