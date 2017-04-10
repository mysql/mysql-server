DROP TABLE IF EXISTS t1, t2;
SET DEBUG_SYNC= 'RESET';
#
# Bug#42438: Crash ha_partition::change_table_ptr
# Test when remove partitioning is done while drop table is waiting
# for the table.
# After MDL was introduced, there is no longer any race, so test is done
# by adding a small sleep to verify that the delete waits. This happens
# only until ALTER tries to upgrade its MDL lock, which ends up in MDL
# deadlock which is correctly reported.
# Con 1
SET DEBUG_SYNC= 'RESET';
CREATE TABLE t1
(a INTEGER,
b INTEGER NOT NULL,
KEY (b))
/*!50100  PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (2),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (100),
PARTITION p3 VALUES LESS THAN MAXVALUE ) */;
SET SESSION debug= "+d,sleep_before_create_table_no_lock";
SET DEBUG_SYNC= 'alter_table_before_create_table_no_lock SIGNAL removing_partitioning WAIT_FOR waiting_for_alter';
SET DEBUG_SYNC= 'mdl_acquire_lock_wait SIGNAL waiting_for_upgrade';
ALTER TABLE t1 REMOVE PARTITIONING;
# Con default
SET DEBUG_SYNC= 'now WAIT_FOR removing_partitioning';
SET DEBUG_SYNC= 'mdl_acquire_lock_wait SIGNAL waiting_for_alter';
SET DEBUG_SYNC= 'rm_table_no_locks_before_delete_table WAIT_FOR waiting_for_upgrade';
DROP TABLE IF EXISTS t1;
# Con 1
ERROR 40001: Deadlock found when trying to get lock; try restarting transaction
SET SESSION debug= "-d,sleep_before_create_table_no_lock";
SET DEBUG_SYNC= 'RESET';
SET DEBUG_SYNC= 'RESET';
#
# Bug#42438: Crash ha_partition::change_table_ptr
# Test when remove partitioning is failing due to drop table is already
# in progress.
# After MDL was introduced, there is no longer any race, so test is done
# by adding a small sleep to verify that the alter waits.
CREATE TABLE t2
(a INTEGER,
b INTEGER NOT NULL,
KEY (b))
/*!50100  PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (2),
PARTITION p1 VALUES LESS THAN (20),
PARTITION p2 VALUES LESS THAN (100),
PARTITION p3 VALUES LESS THAN MAXVALUE ) */;
SET DEBUG_SYNC= 'alter_table_before_open_tables SIGNAL removing_partitions WAIT_FOR waiting_for_alter';
SET DEBUG_SYNC= 'alter_table_before_rename_result_table WAIT_FOR delete_done';
ALTER TABLE t2 REMOVE PARTITIONING;
# Con default
SET SESSION debug= "+d,sleep_before_no_locks_delete_table";
SET DEBUG_SYNC= 'now WAIT_FOR removing_partitions';
SET DEBUG_SYNC= 'rm_table_no_locks_before_delete_table SIGNAL waiting_for_alter';
SET DEBUG_SYNC= 'rm_table_no_locks_before_binlog SIGNAL delete_done';
DROP TABLE IF EXISTS t2;
SET SESSION debug= "-d,sleep_before_no_locks_delete_table";
# Con 1
ERROR 42S02: Table 'test.t2' doesn't exist
SET DEBUG_SYNC= 'RESET';
# Con default
SET DEBUG_SYNC= 'RESET';
End of 5.1 tests
#
# Test Auto increment generation and release of overbooked values.
#
CREATE TABLE t1 (a SERIAL) ENGINE = InnoDB
PARTITION BY KEY ALGORITHM = 1 () PARTITIONS 3;
INSERT INTO t1 VALUES (10);
SET DEBUG_SYNC="release_auto_increment SIGNAL auto_inc_held WAIT_FOR release";
INSERT INTO t1 VALUES (3), (NULL), (4);
# Con1
SET DEBUG_SYNC="now WAIT_FOR auto_inc_held";
INSERT INTO t1 VALUES (5);
# Not really a bug, since no row has been assigned the number between
# this and the next auto_inc value.
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
14
SET DEBUG_SYNC="now SIGNAL release";
# Default
# Reaping INSERT INTO t1 VALUES (3), (NULL), (4);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
12
# Test with generating an auto inc value in Con1
TRUNCATE TABLE t1;
INSERT INTO t1 VALUES (10);
SET DEBUG_SYNC="release_auto_increment SIGNAL auto_inc_held WAIT_FOR release";
INSERT INTO t1 VALUES (3), (NULL), (4);
# Con1
SET DEBUG_SYNC="now WAIT_FOR auto_inc_held";
INSERT INTO t1 VALUES (NULL);
# Not really a bug, since no row has been assigned the number between
# this and the next auto_inc value.
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
15
SET DEBUG_SYNC="now SIGNAL release";
# Default
# Reaping INSERT INTO t1 VALUES (3), (NULL), (4);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
15
#
# Test with INSERT SELECT
#
TRUNCATE TABLE t1;
CREATE TABLE t2 (a INT);
INSERT INTO t1 VALUES (10);
INSERT INTO t2 VALUES (3), (NULL), (4);
SET DEBUG_SYNC="release_auto_increment SIGNAL auto_inc_held WAIT_FOR release";
INSERT INTO t1 SELECT * FROM t2;
# Con1
SET DEBUG_SYNC="now WAIT_FOR auto_inc_held";
INSERT INTO t1 VALUES (5);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
12
SET DEBUG_SYNC="now SIGNAL release";
# Default
# Reaping: INSERT INTO t1 SELECT * FROM t2;
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
12
# Test with generating an auto inc value in Con1
TRUNCATE TABLE t1;
INSERT INTO t1 VALUES (10);
SET DEBUG_SYNC="release_auto_increment SIGNAL auto_inc_held WAIT_FOR release";
INSERT INTO t1 SELECT * FROM t2;
# Con1
SET DEBUG_SYNC="now WAIT_FOR auto_inc_held";
INSERT INTO t1 VALUES (NULL);
# Not really a bug, since no row has been assigned the number between
# this and the next auto_inc value.
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
13
SET DEBUG_SYNC="now SIGNAL release";
# Default
# Reaping: INSERT INTO t1 SELECT * FROM t2;
SELECT AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 't1';
AUTO_INCREMENT
13
DROP TABLE t1, t2;
