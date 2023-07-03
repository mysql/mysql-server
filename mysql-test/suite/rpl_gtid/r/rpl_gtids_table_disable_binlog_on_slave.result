include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE IF NOT EXISTS t1 (a INT) ENGINE=InnoDB;
CREATE TABLE t2 (a INT) ENGINE=MyISAM;
#
# Verify that XA transactions' gtids can be reported
# from global.gtid_executed correctly.
#
INSERT INTO t2 VALUES(1);
XA START '1';
INSERT INTO t2 VALUES(2);
INSERT INTO t1 VALUES(1);
INSERT INTO t1 VALUES(2);
XA END '1';
XA PREPARE '1';
XA COMMIT '1';
include/assert.inc [committed gtids MASTER_UUID:1-6]
#
# Verify that specified gtid for XA transaction can be reported
# from global.gtid_executed correctly.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:17';
XA START '1';
INSERT INTO t1 VALUES(3);
XA END '1';
XA PREPARE '1';
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:18';
XA COMMIT '1';
include/assert.inc [committed gtids MASTER_UUID:1-6:17-18]
#
# Verify that we can store gtids MASTER_UUID:1-6:16 in gtid_executed
# table for XA transactions on binlog rotation.
#
SET GTID_NEXT='AUTOMATIC';
FLUSH LOGS;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	6
MASTER_UUID	17	18
include/sync_slave_sql_with_master.inc
#
# connection slave
#
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
call mtr.add_suppression("Slave SQL for channel '': Error in Xid_log_event: Commit could not "
                         "be completed, 'Got error -1 - 'Unknown error.*' from storage engine', "
                         "Error_code: MY-001030");
call mtr.add_suppression("Slave.*Got error -1 .* from storage engine.*Error_code: MY-001030");
call mtr.add_suppression("Slave: An error occurred at storing the GTID value");
call mtr.add_suppression("The slave coordinator and worker threads are stopped");
include/assert.inc [committed gtids MASTER_UUID:1-6:17-18]
#
# Verify that the transaction is skiped if its specified gtid
# is already in global.gtid_executed.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:4';
INSERT INTO t1 VALUES(11);
include/assert.inc [Table t1 must not contain 11]
#
# Verify that the specified gtid to GTID_NEXT is stored into
# global.gtid_executed and gtid_executed table.
#
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:19';
COMMIT;
include/assert.inc [committed gtids MASTER_UUID:1-6:17-19]
#
# Verify that the gtid_executed table is compressed.
#
SET @debug_save= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,compress_gtid_table';
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:20';
COMMIT;
SET GTID_NEXT='AUTOMATIC';
# Trigger compression explicitly
FLUSH LOGS;
SET DEBUG_SYNC='now WAIT_FOR complete_compression';
SET GLOBAL DEBUG= @debug_save;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	6
MASTER_UUID	17	20
#
# Verify that gtid_executed and gtid_purged are always same
# when binlog is disabled.
#
include/assert.inc [gtid_executed and gtid_purged are always same when binlog is disabled]
#
# Verify that no gtid is generated for transaction
# when binlog is disabled
#
CREATE TABLE t3 (a INT);
INSERT INTO t1 VALUES(12);
DROP TABLE t3;
#
# Stored gtids MASTER_UUID:1-6:17-20 in gtid_executed table
# on slave.
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	6
MASTER_UUID	17	20
SET @@SESSION.GTID_NEXT = 'automatic';
XA START   'xid_1';
INSERT INTO t1 VALUES(1);
XA END     'xid_1';
XA PREPARE 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
XA COMMIT 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
SET @@SESSION.GTID_NEXT = 'automatic';
XA START   'xid_1';
INSERT INTO t1 VALUES(1);
XA END     'xid_1';
XA PREPARE 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
XA ROLLBACK 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
SET @@SESSION.GTID_NEXT = 'automatic';
XA START   'xid_1';
INSERT INTO t1 VALUES(1);
XA END     'xid_1';
XA PREPARE 'xid_1';
SET @@SESSION.GTID_NEXT = 'MASTER_UUID:21';
XA ROLLBACK 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
SET @@SESSION.GTID_NEXT = 'MASTER_UUID:22';
XA START   'xid_1';
INSERT INTO t1 VALUES(1);
XA END     'xid_1';
XA PREPARE 'xid_1';
XA ROLLBACK 'xid_1';
ERROR HY000: When @@SESSION.GTID_NEXT is set to a GTID, you must explicitly set it to a different value after a COMMIT or ROLLBACK. Please check GTID_NEXT variable manual page for detailed explanation. Current @@SESSION.GTID_NEXT is 'MASTER_UUID:22'.
SET @@SESSION.GTID_NEXT = 'automatic';
XA ROLLBACK 'xid_1';
SET @@SESSION.GTID_NEXT = 'automatic';
DROP TABLE t1, t2;
include/sync_slave_sql_with_master.inc
CREATE TABLE t3 (a INT);
include/sync_slave_sql_with_master.inc
FLUSH LOGS;
#
# Verify that transaction's gtid is not stored into gtid_executed table
# and the transaction is rolled back if we encounter an error
# when writing gtid into table when binlog is disabled and
# gtid_mode is enabled.
#
SET @@GLOBAL.DEBUG= '+d,simulate_err_on_write_gtid_into_table';
INSERT INTO t3 VALUES(1);
include/wait_for_slave_sql_error.inc [errno=1030]
SET GLOBAL DEBUG= @debug_save;
include/stop_slave_io.inc
#
# Stored gtids MASTER_UUID:1-15:17-22 in gtid_executed table on slave.
#
include/assert.inc [committed gtids MASTER_UUID:1-15:17-22]
include/assert.inc [Table t3 must not contain 1]
#
# Verify that the gtid_executed table is compressed.
#
SET @debug_save= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,compress_gtid_table';
SET @@SESSION.GTID_NEXT= 'MASTER_UUID:23';
COMMIT;
SET GTID_NEXT='AUTOMATIC';
# Trigger compression explicitly
FLUSH LOGS;
SET DEBUG_SYNC='now WAIT_FOR complete_compression';
SET GLOBAL DEBUG= @debug_save;
SELECT * FROM mysql.gtid_executed;
source_uuid	interval_start	interval_end
MASTER_UUID	1	15
MASTER_UUID	17	23
DROP TABLE t3;
CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
DROP TABLE t3;
CALL mtr.add_suppression("Statement is unsafe because it is being used inside a XA transaction");
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/rpl_end.inc
