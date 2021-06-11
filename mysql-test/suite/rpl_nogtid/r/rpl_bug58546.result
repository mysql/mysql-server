include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

# Bug#58546 test rpl_packet timeout failure sporadically on PB
# ----------------------------------------------------------------------
# STOP SLAVE stopped IO thread first and then stopped SQL thread. It was
# possible that IO thread stopped after replicating part of a transaction
# which SQL thread was executing. SQL thread would be hung if the
# transaction could not be rolled back safely.
# It caused some sporadic failures on PB2.
#
# This test verifies that when 'STOP SLAVE' is issued by a user, IO
# thread will continue to fetch the rest events of the transaction which
# is being executed by SQL thread and is not able to be rolled back safely.
CREATE TABLE t1 (c1 INT KEY, c2 INT) ENGINE=InnoDB;
CREATE TABLE t2 (c1 INT) ENGINE=MyISAM;
INSERT INTO t1 VALUES(1, 1);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
[connection master]
include/stop_dump_threads.inc
# Adding debug point 'dump_thread_wait_before_send_xid' to @@GLOBAL.debug
[connection slave]
include/start_slave.inc
BEGIN;
UPDATE t1 SET c2 = 2 WHERE c1 = 1;
[connection master]
BEGIN;
INSERT INTO t1 VALUES(2, 2);
INSERT INTO t2 VALUES(1);
UPDATE t1 SET c2 = 3 WHERE c1 = 1;
COMMIT;
[connection slave1]
STOP SLAVE;
[connection slave]
ROLLBACK;
[connection master]
SET DEBUG_SYNC= 'now SIGNAL signal.continue';
SET DEBUG_SYNC= 'RESET';
[connection slave]
include/wait_for_slave_to_stop.inc
[connection slave1]
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
[connection master]
# Removing debug point 'dump_thread_wait_before_send_xid' from @@GLOBAL.debug
include/stop_dump_threads.inc
[connection slave1]
include/start_slave.inc
[connection master]
DROP TABLE t1, t2;
include/rpl_end.inc
