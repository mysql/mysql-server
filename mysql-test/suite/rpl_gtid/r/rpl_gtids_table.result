include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @debug_save= @@GLOBAL.DEBUG;
call mtr.add_suppression("test the suppression statement");
SHOW CREATE TABLE mysql.gtid_executed;
Table	Create Table
gtid_executed	CREATE TABLE `gtid_executed` (
  `source_uuid` char(36) NOT NULL COMMENT 'uuid of the source where the transaction was originally executed.',
  `interval_start` bigint NOT NULL COMMENT 'First number of interval.',
  `interval_end` bigint NOT NULL COMMENT 'Last number of interval.',
  PRIMARY KEY (`source_uuid`,`interval_start`)
) /*!50100 TABLESPACE `mysql` */ ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci STATS_PERSISTENT=0 ROW_FORMAT=DYNAMIC
#
# Verify that the specified gtid to GTID_NEXT can be reported from
# global.gtid_executed and is stored into gtid_executed table on
# following binlog rotation.
#
SET GTID_NEXT='MASTER_UUID:3';
BEGIN;
COMMIT;
include/assert.inc [committed gtids MASTER_UUID:1:3]
SET GTID_NEXT='AUTOMATIC';
FLUSH LOGS;
#
# Store gtids MASTER_UUID:1:3 in gtid_executed table on binlog rotation
#
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	1
MASTER_UUID	3	3
#
# Verify that these gtids can be reported from global.gtid_executed and
# are stored into gtid_executed table on next rotation for normal DDLs.
#
CREATE TABLE IF NOT EXISTS t1 (a INT) ENGINE=InnoDB;
CREATE TABLE t2 (a INT) ENGINE=MyISAM;
include/assert.inc [committed gtids MASTER_UUID:1-4]
#
# Store gtids MASTER_UUID:1-4 in gtid_executed table on binlog rotation
#
SET @@GLOBAL.DEBUG= '+d,compress_gtid_table';
FLUSH LOGS;
SET DEBUG_SYNC='now WAIT_FOR complete_compression';
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	4
#
# Verify that these gtids can be reported from global.gtid_executed
# and are stored into gtid_executed table for compound statement with
# regular and temporary tables.
#
CREATE TEMPORARY TABLE tmp1 (c1 INT) Engine=MyISAM;
CREATE TABLE t3 (a INT);
DROP TEMPORARY TABLE tmp1, t3;
ERROR 42S02: Unknown table 'test.t3'
DROP TEMPORARY TABLE tmp1;
DROP TABLE t3;
include/assert.inc [committed gtids MASTER_UUID:1-8]
#
# Store gtids MASTER_UUID:1-8 in gtid_executed table on binlog rotation
#
SET @@GLOBAL.DEBUG= '+d,compress_gtid_table';
FLUSH LOGS;
SET DEBUG_SYNC='now WAIT_FOR complete_compression';
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	8
#
# Verify that transactions' gtids can be reported from
# global.gtid_executed correctly and are stored into
# gtid_executed table on next binlog rotation.
#
BEGIN;
INSERT INTO t2 VALUES(2);
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
COMMIT;
#
# Verify that specified gtid for transaction can be reported from
# global.gtid_executed correctly and is stored into gtid_executed
# table on next binlog rotation.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:11';
BEGIN;
INSERT INTO t1 VALUES(3);
COMMIT;
include/assert.inc [committed gtids MASTER_UUID:1-11]
SET GTID_NEXT='AUTOMATIC';
SET @@GLOBAL.DEBUG= '+d,compress_gtid_table';
FLUSH LOGS;
SET DEBUG_SYNC='now WAIT_FOR complete_compression';
#
# Store gtids MASTER_UUID:1-11 in gtid_executed table on binlog rotation
#
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	11
#
# Verify that transaction's gtid can not be reported from
# global.gtid_executed and is not stored into gtid_executed table
# on next binlog rotation if the transaction is rollbacked.
#
BEGIN;
INSERT INTO t1 VALUES(4);
ROLLBACK;
include/assert.inc [committed gtids MASTER_UUID:1-11]
#
# Store gtids MASTER_UUID:1-11 in gtid_executed table on binlog rotation
#
FLUSH LOGS;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	11
SET GLOBAL DEBUG= @debug_save;
include/sync_slave_sql_with_master.inc
#
# connection slave
#
#
# Verify that the transaction is skiped if its specified gtid is
# already in global.gtid_executed, although not in gtid_executed table.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:6';
INSERT INTO t1 VALUES(11);
include/assert.inc [Table t1 must not contain 11]
include/assert.inc [committed gtids MASTER_UUID:1-11]
#
# The gtid_executed table would only have GTID for Innodb transactions
#
#
# Store gtids MASTER_UUID:1-11 in gtid_executed table on binlog rotation
#
SET GTID_NEXT='AUTOMATIC';
FLUSH LOGS;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	11
#
# Verify that the specified gtid to GTID_NEXT is stored into
# global.gtid_executed immediately and stored into gtid_executed table
# on next binlog rotation.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:17';
COMMIT;
include/assert.inc [committed gtids MASTER_UUID:1-11:17]
#
# Store gtids MASTER_UUID:1-11:17 in gtid_executed table
# on next binlog rotation.
#
SET GTID_NEXT='AUTOMATIC';
FLUSH LOGS;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	11
MASTER_UUID	17	17
#
# Verify that we can get the correct set of gtid_purged
# when purging logs.
#
PURGE BINARY LOGS TO 'slave-bin.000003';
include/assert.inc [purged gtids MASTER_UUID:1-11:17]
#
# Verify that transaction's gtid generated on slave is stored
# into gtid_executed table on next binlog rotation.
#
BEGIN;
INSERT INTO t1 VALUES(12);
COMMIT;
#
# Store gtids MASTER_UUID:1-11:17 and SLAVE_UUID:1
# in gtid_executed table on binlog rotation.
#
FLUSH LOGS;
SELECT * FROM mysql.gtid_executed where source_uuid="MASTER_UUID";
source_uuid	interval_start	interval_end
MASTER_UUID	1	11
MASTER_UUID	17	17
SELECT * FROM mysql.gtid_executed where source_uuid="SLAVE_UUID";
source_uuid	interval_start	interval_end
SLAVE_UUID	1	1
DROP TABLE t1, t2;
include/rpl_end.inc
