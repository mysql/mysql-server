include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression('Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT.');
CALL mtr.add_suppression("Error dropping database");
CALL mtr.add_suppression("Can't drop database '.*'; database doesn't exist");
CALL mtr.add_suppression("Slave SQL for channel '': ... The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state. .* Error_code: 1756");
==== Case 1: CALL is split on master ====
---- Initialize ----
CREATE TABLE t (a INT);
CREATE PROCEDURE proc ()
BEGIN
INSERT INTO t VALUES (1);
INSERT INTO t VALUES (2);
END|
---- GTID_NEXT=AUTOMATIC ----
include/gtid_step_reset.inc
CALL proc();
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert.inc [Both rows were inserted]
DELETE FROM t;
---- GTID_NEXT=non-automatic ----
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
CALL proc();
SET GTID_NEXT= 'AUTOMATIC';
include/gtid_step_assert.inc [count=1, only_count=0]
include/assert.inc [One row inserted if GTID_MODE=ON, two if GTID_MODE=OFF]
DROP TABLE t;
DROP PROCEDURE proc;
==== Case 2A: DROP TABLE is split on master ====
---- Initialize ----
CREATE PROCEDURE create_tables()
BEGIN
CREATE TABLE base (a INT) ENGINE = InnoDB;
CREATE TEMPORARY TABLE temp_t (a INT) ENGINE = InnoDB;
CREATE TEMPORARY TABLE temp_n (a INT) ENGINE = MyISAM;
END|
CREATE PROCEDURE drop_tables()
BEGIN
DROP TABLE IF EXISTS base;
DROP TABLE IF EXISTS temp_t;
DROP TABLE IF EXISTS temp_n;
END|
include/rpl_sync.inc
---- GTID_MODE=AUTOMATIC ----
CALL create_tables();
include/gtid_step_reset.inc
DROP TABLE base, temp_t;
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_n;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
DROP TABLE base, temp_n;
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_t;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
DROP TABLE temp_t, temp_n;
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE base;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
DROP TABLE base, temp_t, temp_n;
include/gtid_step_assert.inc [count=3, only_count=0]
include/assert_binlog_events.inc
include/rpl_sync.inc
include/rpl_reset.inc
---- GTID_MODE=non-automatic ----
CALL create_tables();
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
DROP TABLE base, temp_t;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_n;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
DROP TABLE base, temp_n;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_t;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
DROP TABLE temp_t, temp_n;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=2, only_count=0]
include/assert_binlog_events.inc
DROP TABLE base;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
DROP TABLE base, temp_t, temp_n;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=3, only_count=0]
include/assert_binlog_events.inc
include/rpl_sync.inc
include/rpl_reset.inc
==== Case 2B: DROP TABLE is split on slave ====
---- Initialize ----
CREATE TABLE dummy (a INT);
DROP PROCEDURE create_tables;
CREATE FUNCTION create_tables_func() RETURNS INT
BEGIN
IF @@GLOBAL.SERVER_ID = 2 THEN
CREATE TEMPORARY TABLE temp_t (a INT) ENGINE = InnoDB;
CREATE TEMPORARY TABLE temp_n (a INT) ENGINE = MyISAM;
END IF;
RETURN 0;
END|
CREATE PROCEDURE create_tables()
BEGIN
CREATE TABLE base (a INT);
SET @@SESSION.SQL_LOG_BIN = 0;
CREATE TABLE temp_t (a INT);
CREATE TABLE temp_n (a INT);
SET @@SESSION.SQL_LOG_BIN = 1;
INSERT INTO dummy VALUES (create_tables_func());
END|
include/rpl_sync.inc
---- GTID_MODE=AUTOMATIC ----
CALL create_tables();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
include/gtid_step_reset.inc
DROP TABLE base, temp_t;
include/gtid_step_assert.inc [count=1, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_n;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
include/gtid_step_reset.inc
DROP TABLE base, temp_n;
include/gtid_step_assert.inc [count=1, only_count=0]
include/assert_binlog_events.inc
DROP TABLE temp_t;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
include/gtid_step_reset.inc
DROP TABLE temp_t, temp_n;
include/gtid_step_assert.inc [count=1, only_count=0]
include/assert_binlog_events.inc
DROP TABLE base;
include/rpl_sync.inc
include/rpl_reset.inc
CALL create_tables();
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
include/gtid_step_reset.inc
DROP TABLE base, temp_t, temp_n;
include/gtid_step_assert.inc [count=1, only_count=0]
include/assert_binlog_events.inc
include/rpl_sync.inc
include/rpl_reset.inc
---- Clean up ----
DROP FUNCTION create_tables_func;
DROP PROCEDURE create_tables;
DROP PROCEDURE drop_tables;
DROP TABLE dummy;
==== Case 2C: DROP TABLES for base tables in engines with and without atomic DDL support ====
CREATE TABLE base_1_n (a INT) ENGINE = MyISAM;
CREATE TABLE base_2_n (a INT) ENGINE = MyISAM;
CREATE TABLE base_3_a (a INT) ENGINE = InnoDB;
CREATE TABLE base_4_a (a INT) ENGINE = InnoDB;
include/rpl_sync.inc
---- GTID_MODE=AUTOMATIC ----
include/gtid_step_reset.inc
DROP TABLES base_1_n, base_2_n, base_3_a, base_4_a;
# In AUTOMATIC mode the above statement should be split into three:
# two statements for each of MyISAM tables and single statement
# dropping both of InnoDB tables.
include/gtid_step_assert.inc [count=3, only_count=0]
CREATE TABLE base_1_n (a INT) ENGINE = MyISAM;
CREATE TABLE base_2_n (a INT) ENGINE = MyISAM;
CREATE TABLE base_3_a (a INT) ENGINE = InnoDB;
CREATE TABLE base_4_a (a INT) ENGINE = InnoDB;
include/rpl_sync.inc
---- GTID_NEXT=non-automatic ----
include/gtid_step_reset.inc
SET GTID_NEXT = '#';
DROP TABLES base_1_n, base_2_n, base_3_a, base_4_a;
SET GTID_NEXT= 'AUTOMATIC';
# Without GTID assigned the above statement should be split into
# three as in AUTOMATIC mode.
include/gtid_step_assert.inc [count=3, only_count=0]
include/rpl_sync.inc
==== Case 2D: Failing DROP TABLES for base tables with and without atomic DDL support ====
See rpl_split_statements_debug.test
==== Case 3: DROP DATABASE ====
See rpl_split_statements_debug.test
==== Case 4: CREATE TABLE ... SELECT ====
See rpl_gtid_create_select.test
include/rpl_end.inc
