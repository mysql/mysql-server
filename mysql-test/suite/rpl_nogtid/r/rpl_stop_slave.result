include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]

# BUG#56118 STOP SLAVE does not wait till trx with CREATE TMP TABLE ends
#
# If a temporary table is created or dropped, the transaction should be
# regarded similarly that a non-transactional table is modified. So 
# STOP SLAVE should wait until the transaction has finished.
CREATE TABLE t1(c1 INT) ENGINE=InnoDB;
CREATE TABLE t2(c1 INT) ENGINE=InnoDB;
include/sync_slave_sql_with_master.inc
SET DEBUG_SYNC= 'RESET';
include/stop_slave.inc

# Suspend the INSERT statement in current transaction on SQL thread.
# It guarantees that SQL thread is applying the transaction when
# STOP SLAVE command launchs.
SET GLOBAL debug= '+d,after_mysql_insert';
include/start_slave.inc

# CREATE TEMPORARY TABLE with InnoDB engine
# -----------------------------------------

[ On Master ]
BEGIN;
DELETE FROM t1;
CREATE TEMPORARY TABLE tt1(c1 INT) ENGINE = InnoDB;
INSERT INTO t1 VALUES (1);
DROP TEMPORARY TABLE tt1;
COMMIT;

[ On Slave ]
STOP SLAVE SQL_THREAD;

[ On Slave1 ]
# To resume slave SQL thread
SET DEBUG_SYNC= 'now SIGNAL signal.continue';
SET DEBUG_SYNC= 'RESET';

[ On Slave ]
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
include/wait_for_slave_sql_to_stop.inc
# Slave should stop after the transaction has committed.
# So t1 on master is same to t1 on slave.
include/diff_tables.inc [master:t1, slave:t1]
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_start.inc

# CREATE TEMPORARY TABLE ... SELECT with InnoDB engine
# ----------------------------------------------------

[ On Master ]
BEGIN;
DELETE FROM t1;
CREATE TEMPORARY TABLE tt1(c1 INT) ENGINE = InnoDB
SELECT c1 FROM t2;
INSERT INTO t1 VALUES (1);
DROP TEMPORARY TABLE tt1;
COMMIT;

[ On Slave ]
STOP SLAVE SQL_THREAD;

[ On Slave1 ]
# To resume slave SQL thread
SET DEBUG_SYNC= 'now SIGNAL signal.continue';
SET DEBUG_SYNC= 'RESET';

[ On Slave ]
Warnings:
Warning	1287	'STOP SLAVE' is deprecated and will be removed in a future release. Please use STOP REPLICA instead
include/wait_for_slave_sql_to_stop.inc
# Slave should stop after the transaction has committed.
# So t1 on master is same to t1 on slave.
include/diff_tables.inc [master:t1, slave:t1]
START SLAVE SQL_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_sql_to_start.inc

# Test end
SET GLOBAL debug= '-d,after_mysql_insert';
include/restart_slave.inc
call mtr.add_suppression("Slave SQL.*Request to stop slave SQL Thread received while applying a group that has non-transactional changes; waiting for completion of the group");
call mtr.add_suppression("The slave coordinator and worker threads are stopped, possibly leaving data in inconsistent state");
[connection master]
DROP TABLE t1, t2;
include/rpl_end.inc
