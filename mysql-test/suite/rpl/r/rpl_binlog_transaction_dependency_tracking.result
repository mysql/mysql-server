include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#### INITIALIZE ####
call mtr.add_suppression("Transaction is tagged with inconsistent logical timestamps");
call mtr.add_suppression("Cannot execute the current event group in the parallel mode.");
include/stop_slave.inc
SET @save_replica_parallel_type = @@GLOBAL.replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @save_replica_parallel_workers = @@GLOBAL.replica_parallel_workers;
SET @save_replica_transaction_retries = @@GLOBAL.replica_transaction_retries;
SET GLOBAL replica_parallel_type = LOGICAL_CLOCK;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET GLOBAL replica_parallel_workers = 3;
SET GLOBAL replica_transaction_retries = 0;
SET @save_binlog_transaction_dependency_tracking = @@GLOBAL.binlog_transaction_dependency_tracking;
SET @save_binlog_transaction_dependency_history_size = @@GLOBAL.binlog_transaction_dependency_history_size;
SET GLOBAL binlog_transaction_dependency_tracking = COMMIT_ORDER;
CREATE TABLE tests (`id` INT NOT NULL, `description` VARCHAR(50), `results` VARCHAR(100), PRIMARY KEY(`id`));
INSERT INTO `tests` (`id`, `description`, `results`) VALUES
(1,  'Writeset',                  '0 1;1 2;1 3;1 4;3 5;3 6;4 7;7 8;6 9;9 10'),
(2,  'Writeset+DDL',              '0 1;1 2;1 3;1 4;4 5;5 6;6 7;7 8;4 9;7 10;10 11;9 12;12 13'),
(3,  'Writeset+rotation',         '0 1;0 2;1 3;3 4;2 5;5 6'),
(4,  'Writeset+history',          '0 1;1 2;1 3;1 4;4 5;4 6;6 7;7 8;6 9;9 10'),
(5,  'Writeset_session',          '0 1;1 2;1 3;2 4;3 5;3 6;5 7;7 8;7 9;9 10'),
(6,  'Writeset_session+DDL',      '0 1;1 2;1 3;2 4;4 5;5 6;6 7;7 8;4 9;8 10;10 11;10 12;12 13'),
(7,  'Writeset_session+rotation', '0 1;0 2;1 3;3 4;3 5;5 6'),
(8,  'Writeset_session+history',  '0 1;1 2;1 3;2 4;4 5;4 6;6 7;7 8;7 9;9 10'),
(9,  'Commit_order',              '0 1;1 2;1 3;3 4;4 5;4 6;6 7;7 8;7 9;9 10'),
(10, 'Commit_order+DDL',          '0 1;1 2;1 3;3 4;4 5;5 6;6 7;7 8;4 9;9 10;10 11;10 12;12 13'),
(11, 'Commit_order+rotation',     '0 1;0 2;2 3;3 4;3 5;5 6'),
(12, 'Commit_order+history',      '0 1;1 2;1 3;3 4;4 5;4 6;6 7;7 8;7 9;9 10');
FLUSH LOGS;
#### TEST ####
SET GLOBAL binlog_transaction_dependency_tracking = WRITESET;
######## 1. WRITESET ########
#### STEP 1.1 TEST Writeset ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000002
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;1 4;3 5;3 6;4 7;7 8;6 9;9 10]
#### STEP 1.2 TEST Writeset+DDL ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Inserted a DDL after TRX5 and check that they run in parallel due to commit_order
CREATE TABLE `t2` (`id` int(11) NOT NULL, `val` int(11) NOT NULL) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
ALTER TABLE t2 ADD COLUMN b INT;
DROP TABLE `t2`;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000003
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;1 4;4 5;5 6;6 7;7 8;4 9;7 10;10 11;9 12;12 13]
#### STEP 1.3 TEST Writeset+rotation ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Flushing logs in the middle of the set
FLUSH LOGS;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000005
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;0 2;1 3;3 4;2 5;5 6]
#### STEP 1.4 TEST Writeset+history ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
History size reduced to 5 to test when it becomes full
SET GLOBAL binlog_transaction_dependency_history_size=5;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET	5
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000006
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;1 4;4 5;4 6;6 7;7 8;6 9;9 10]
SET GLOBAL binlog_transaction_dependency_tracking = WRITESET_SESSION;
######## 2. WRITESET_SESSION ########
#### STEP 2.1 TEST Writeset_session ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET_SESSION	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000007
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;2 4;3 5;3 6;5 7;7 8;7 9;9 10]
#### STEP 2.2 TEST Writeset_session+DDL ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET_SESSION	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Inserted a DDL after TRX5 and check that they run in parallel due to commit_order
CREATE TABLE `t2` (`id` int(11) NOT NULL, `val` int(11) NOT NULL) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
ALTER TABLE t2 ADD COLUMN b INT;
DROP TABLE `t2`;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000008
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;2 4;4 5;5 6;6 7;7 8;4 9;8 10;10 11;10 12;12 13]
#### STEP 2.3 TEST Writeset_session+rotation ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET_SESSION	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Flushing logs in the middle of the set
FLUSH LOGS;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000010
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;0 2;1 3;3 4;3 5;5 6]
#### STEP 2.4 TEST Writeset_session+history ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
History size reduced to 5 to test when it becomes full
SET GLOBAL binlog_transaction_dependency_history_size=5;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
WRITESET_SESSION	5
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000011
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;2 4;4 5;4 6;6 7;7 8;7 9;9 10]
SET GLOBAL binlog_transaction_dependency_tracking = COMMIT_ORDER;
######## 3. COMMIT_ORDER ########
#### STEP 3.1 TEST Commit_order ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
COMMIT_ORDER	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000012
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;4 5;4 6;6 7;7 8;7 9;9 10]
#### STEP 3.2 TEST Commit_order+DDL ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
COMMIT_ORDER	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Inserted a DDL after TRX5 and check that they run in parallel due to commit_order
CREATE TABLE `t2` (`id` int(11) NOT NULL, `val` int(11) NOT NULL) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
ALTER TABLE t2 ADD COLUMN b INT;
DROP TABLE `t2`;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000013
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;4 5;5 6;6 7;7 8;4 9;9 10;10 11;10 12;12 13]
#### STEP 3.3 TEST Commit_order+rotation ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
COMMIT_ORDER	25000
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
Flushing logs in the middle of the set
FLUSH LOGS;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000015
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;0 2;2 3;3 4;3 5;5 6]
#### STEP 3.4 TEST Commit_order+history ####
SET GLOBAL binlog_transaction_dependency_history_size=25000;
History size reduced to 5 to test when it becomes full
SET GLOBAL binlog_transaction_dependency_history_size=5;
SELECT @@binlog_transaction_dependency_tracking,
@@binlog_transaction_dependency_history_size;
@@binlog_transaction_dependency_tracking	@@binlog_transaction_dependency_history_size
COMMIT_ORDER	5
CREATE TABLE `t1` (`id` int(11) NOT NULL, `val` int(11) NOT NULL,
PRIMARY KEY (`id`)) ENGINE=InnoDB;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
Warning	1681	Integer display width is deprecated and will be removed in a future release.
#TRX2 (session A, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (1, 0),(2, 0),(3, 0);
#TRX1 (session B, independent)
BEGIN;
INSERT INTO t1(id, val) VALUES (4, 0),(5, 0);
COMMIT;
finish TRX2 after TRX1
COMMIT;
#TRX3 (session B, no data dependency, depends on TRX1 (session))
BEGIN;
INSERT INTO t1(id, val) VALUES (6, 0),(7, 0);
COMMIT;
#TRX5 (session A, depends on TRX2 (session and data) and TRX1(data))
BEGIN;
INSERT INTO t1(id, val) VALUES (8, 1);
UPDATE t1 SET val=val+1 WHERE id=1;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX4 (session C, depends on TRX2 (data))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=3;
INSERT INTO t1(id, val) VALUES (9, 1);
COMMIT;
finish TRX5 after TRX4
COMMIT;
#TRX6 (session C, depends on TRX4 (session) and TRX3(data))
BEGIN;
DELETE FROM t1 WHERE id=6;
COMMIT;
#TRX8 (session C, depends on TRX5 (data) and TRX6 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=4;
#TRX7 (session B, depends on TRX1 and TRX6 (data) and TRX3 (session))
BEGIN;
UPDATE t1 SET val=val+1 WHERE id=5;
INSERT INTO t1(id, val) VALUES (6, 0);
COMMIT;
finish TRX8 after TRX7
COMMIT;
Drop table and flush logs to force binlog to rotate
DROP TABLE t1;
Processing binlog master-bin.000016
FLUSH LOGS;
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;4 5;4 6;6 7;7 8;7 9;9 10]

#### Verify that replication was correct ####
include/start_slave.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:test.tests, slave:test.tests]
DROP TABLE `tests`;
include/sync_slave_sql_with_master.inc
SET GLOBAL binlog_transaction_dependency_tracking= WRITESET;
SET GLOBAL binlog_transaction_dependency_history_size= DEFAULT;
FLUSH LOGS;

#### Bug#25616372, PART1 ####
CREATE TABLE t1 (a INT PRIMARY KEY NOT NULL AUTO_INCREMENT);
INSERT INTO t1 VALUES (null);
INSERT INTO t1 VALUES (null);
set global binlog_transaction_dependency_tracking=WRITESET;
INSERT INTO t1 VALUES (null);
INSERT INTO t1 VALUES (null);
Processing binlog master-bin.000018
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;3 5]
FLUSH LOGS;

#### Bug#25616372, PART2 ####
INSERT INTO t1 VALUES (null);
INSERT INTO t1 VALUES (null);
set global binlog_transaction_dependency_tracking=WRITESET;
INSERT INTO t1 VALUES (null);
INSERT INTO t1 VALUES (null);
INSERT INTO t1 VALUES (null);
Processing binlog master-bin.000019
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3;2 4;2 5]
DROP TABLE t1;
FLUSH LOGS;

#### Bug#26277771, PART1 ####
CREATE TABLE t1 (c1 int(10) NOT NULL, c2 varchar(20) NOT NULL, PRIMARY KEY (c1),
UNIQUE KEY (c1, c2)) ENGINE=InnoDB DEFAULT CHARSET=latin1;
Warnings:
Warning	1681	Integer display width is deprecated and will be removed in a future release.
INSERT INTO t1 VALUES (1, 'a');
DELETE FROM t1 WHERE c1=1 AND c2='a';
INSERT INTO t1 VALUES (1, 'A');
Processing binlog master-bin.000020
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3;3 4]
DROP TABLE t1;
FLUSH LOGS;

#### Bug#26277771, PART2 ####
CREATE TABLE t1 (c1 VARCHAR(20) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL PRIMARY KEY);
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
INSERT INTO t1 VALUES ('a');
INSERT INTO t1 VALUES ('A');
INSERT INTO t1 VALUES ('b');
INSERT INTO t1 VALUES ('B');
Processing binlog master-bin.000021
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;1 4;1 5]
DROP TABLE t1;
FLUSH LOGS;

#### Bug#26277771, PART3 ####
CREATE TABLE t1 (c1 VARCHAR(20) NOT NULL PRIMARY KEY);
INSERT INTO t1 VALUES ('a');
INSERT INTO t1 VALUES ('b');
CREATE TABLE t2 (c1 BINARY(3) NOT NULL PRIMARY KEY);
INSERT INTO t2 VALUES ('a');
INSERT INTO t2 VALUES ('\0a');
INSERT INTO t2 VALUES ('\0');
INSERT INTO t2 VALUES ('a ');
Processing binlog master-bin.000022
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3;3 4;4 5;4 6;4 7;4 8]
DROP TABLE t1;
DROP TABLE t2;
FLUSH LOGS;

#### Bug#26277771, PART4 ####
CREATE TABLE t1 (c1 varchar(20) NOT NULL PRIMARY KEY)
ENGINE=InnoDB CHARACTER SET utf8 COLLATE utf8_bin;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
Warning	3778	'utf8mb3_bin' is a collation of the deprecated character set UTF8MB3. Please consider using UTF8MB4 with an appropriate collation instead.
INSERT INTO t1 VALUES ('a');
INSERT INTO t1 VALUES ('á');
Processing binlog master-bin.000023
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3]
DROP TABLE t1;
FLUSH LOGS;

#### Test ROLLBACK TO SAVEPOINT ####
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE nopk (a INT);
CREATE TABLE parent (a INT PRIMARY KEY);
CREATE TABLE child (a INT PRIMARY KEY, FOREIGN KEY (a) REFERENCES parent(a));
# 1. Hashes of rolled-back rows are removed from writeset
FLUSH LOGS;
INSERT INTO t1 VALUES (1);
BEGIN;
SAVEPOINT sp;
UPDATE t1 SET a = 2;
ROLLBACK TO sp;
INSERT INTO t1 VALUES (2);
COMMIT;
INSERT INTO t1 VALUES (3);
Processing binlog master-bin.000025
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3]
# 2. If transaction is flagged as updating a non-index table, the flag is not rolled-back
FLUSH LOGS;
INSERT INTO t1 VALUES (4);
BEGIN;
SAVEPOINT sp;
INSERT INTO nopk VALUES (1);
ROLLBACK TO sp;
INSERT INTO t1 VALUES (5);
COMMIT;
INSERT INTO t1 VALUES (6);
Processing binlog master-bin.000026
include/include/assert_logical_timestamps.inc [0 1;1 2;1 3]
# 3. If transaction is flagged as updating a foreign key parent table, the flag is not rolled-back
FLUSH LOGS;
INSERT INTO t1 VALUES (7);
BEGIN;
SAVEPOINT sp;
INSERT INTO parent VALUES (1);
ROLLBACK TO sp;
INSERT INTO t1 VALUES (8);
COMMIT;
INSERT INTO t1 VALUES (9);
Processing binlog master-bin.000027
include/include/assert_logical_timestamps.inc [0 1;1 2;2 3]
# Clean up SAVEPOINT tests
DROP TABLE t1;
DROP TABLE nopk;
DROP TABLE child;
DROP TABLE parent;

#### CLEANUP ####
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_type= @save_replica_parallel_type;
Warnings:
Warning	1287	'@@replica_parallel_type' is deprecated and will be removed in a future release.
SET @@GLOBAL.replica_parallel_workers= @save_replica_parallel_workers;
SET @@GLOBAL.replica_transaction_retries= @save_replica_transaction_retries;
include/start_slave.inc
SET GLOBAL binlog_transaction_dependency_tracking = COMMIT_ORDER;
SET @@GLOBAL.binlog_transaction_dependency_tracking= @save_binlog_transaction_dependency_tracking;
SET @@GLOBAL.binlog_transaction_dependency_history_size= @save_binlog_transaction_dependency_history_size;
include/rpl_end.inc
