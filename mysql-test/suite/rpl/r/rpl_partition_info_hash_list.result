include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#####1.Verifying the value of partition_id written to the binary log #####
CREATE TABLE employees (store_id INT NOT NULL) PARTITION BY LIST (store_id)
( PARTITION pEast VALUES IN (4, 8, 12), PARTITION pWest VALUES IN (3, 6, 9),
PARTITION pNorth VALUES IN (5, 10, 15));
include/save_binlog_position.inc
INSERT INTO employees VALUES(10);
include/rpl_partition_info.inc
ALTER TABLE employees ADD PARTITION (PARTITION pSouth VALUES IN (7, 14, 28));
include/save_binlog_position.inc
INSERT INTO employees VALUES(28);
include/rpl_partition_info.inc
#####2.INSERT, UPDATE, DELETE in a partitioned table affecting same and multiple partition #####
#####3.Verifying the number of events created when executing statements affecting same and multiple partitions #####
-----INSERT-----
include/save_binlog_position.inc
include/save_binlog_position.inc
INSERT INTO employees VALUES(4), (8);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
INSERT INTO employees VALUES(4), (6);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
-----UPDATE-----
include/save_binlog_position.inc
UPDATE employees SET store_id = 9 WHERE store_id = 4 OR store_id = 8 ;
include/assert_binlog_events.inc
include/rpl_partition_info_update.inc
include/save_binlog_position.inc
UPDATE employees SET store_id = 8 WHERE store_id = 6 OR store_id = 10;
include/assert_binlog_events.inc
include/rpl_partition_info_update.inc
-----DELETE-----
include/save_binlog_position.inc
DELETE FROM employees where store_id = 8;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
DELETE FROM employees where store_id = 9 OR store_id = 28;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
DROP TABLE employees;
CREATE TABLE employees (store_id INT NOT NULL);
ALTER TABLE employees PARTITION BY LIST (store_id) ( PARTITION pEast VALUES IN (4, 8, 12),
PARTITION pWest VALUES IN (3, 6, 9),
PARTITION pNorth VALUES IN (5, 10, 15));
include/save_binlog_position.inc
INSERT INTO employees VALUES(10);
include/rpl_partition_info.inc
ALTER TABLE employees ADD PARTITION (PARTITION pSouth VALUES IN (7, 14, 28));
include/save_binlog_position.inc
INSERT INTO employees VALUES(28);
include/rpl_partition_info.inc
#####2.INSERT, UPDATE, DELETE in a partitioned table affecting same and multiple partition #####
#####3.Verifying the number of events created when executing statements affecting same and multiple partitions #####
-----INSERT-----
include/save_binlog_position.inc
include/save_binlog_position.inc
INSERT INTO employees VALUES(4), (8);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
INSERT INTO employees VALUES(4), (6);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
-----UPDATE-----
include/save_binlog_position.inc
UPDATE employees SET store_id = 9 WHERE store_id = 4 OR store_id = 8 ;
include/assert_binlog_events.inc
include/rpl_partition_info_update.inc
include/save_binlog_position.inc
UPDATE employees SET store_id = 8 WHERE store_id = 6 OR store_id = 10;
include/assert_binlog_events.inc
include/rpl_partition_info_update.inc
-----DELETE-----
include/save_binlog_position.inc
DELETE FROM employees where store_id = 8;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
DELETE FROM employees where store_id = 9 OR store_id = 28;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
DROP TABLE employees;
#####4.Verifying the value of partition_id written to the binary log in case of sub-partitions######
CREATE TABLE sp (id INT, age INT) PARTITION BY LIST (age) SUBPARTITION BY HASH (id)
SUBPARTITIONS 2 (
PARTITION p0 VALUES IN (2, 3, 5, 7, 11),
PARTITION p1 VALUES IN (4, 6, 8, 10, 12));
include/save_binlog_position.inc
INSERT INTO sp VALUES(101, 3);
include/rpl_partition_info.inc
INSERT INTO sp VALUES (102, 3);
INSERT INTO sp VALUES (107, 4);
INSERT INTO sp VALUES (108, 10);
#####5.INSERTS, UPDATES, DELETES in a partitioned table affecting same and multiple sub_partition #####
#####6.Verifying the number of events created when executing statements affecting single and multiple sub_partitions #####
-----INSERT-----
include/save_binlog_position.inc
INSERT INTO sp VALUES(103, 2), (105, 5);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
INSERT INTO sp VALUES(103, 2), (104, 7);
include/assert_binlog_events.inc
include/rpl_partition_info.inc
-----UPDATE-----
include/save_binlog_position.inc
UPDATE sp SET age=5 WHERE id = 102 OR id=104;
include/rpl_partition_info_update.inc
include/assert_binlog_events.inc
include/save_binlog_position.inc
UPDATE sp SET age=11 WHERE id = 102 OR id=105;
include/assert_binlog_events.inc
include/rpl_partition_info_update.inc
-----DELETE-----
include/save_binlog_position.inc
DELETE FROM sp WHERE id = 101 OR id=105;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
include/save_binlog_position.inc
DELETE FROM sp WHERE id = 108 OR id=107;
include/assert_binlog_events.inc
include/rpl_partition_info.inc
#####7.Sync with slave and check if all the tables are replicated correctly#####
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:sp, slave:sp]
DROP TABLE sp;
RESET MASTER;
include/rpl_end.inc
