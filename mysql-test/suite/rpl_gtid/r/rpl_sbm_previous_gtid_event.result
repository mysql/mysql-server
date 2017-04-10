include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @save_global_debug=@@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,dbug.calculate_sbm_after_previous_gtid_log_event';
SET @saved_slave_transaction_retries=@@global.slave_transaction_retries;
SET @@global.slave_transaction_retries=0;
include/start_slave_io.inc
include/stop_slave_io.inc
include/start_slave_io.inc
CREATE TABLE t1(i INT);
INSERT INTO t1 VALUES (20);
INSERT INTO t1 VALUES (19);
INSERT INTO t1 VALUES (18);
INSERT INTO t1 VALUES (17);
INSERT INTO t1 VALUES (16);
INSERT INTO t1 VALUES (15);
INSERT INTO t1 VALUES (14);
INSERT INTO t1 VALUES (13);
INSERT INTO t1 VALUES (12);
INSERT INTO t1 VALUES (11);
INSERT INTO t1 VALUES (10);
INSERT INTO t1 VALUES (9);
INSERT INTO t1 VALUES (8);
INSERT INTO t1 VALUES (7);
INSERT INTO t1 VALUES (6);
INSERT INTO t1 VALUES (5);
INSERT INTO t1 VALUES (4);
INSERT INTO t1 VALUES (3);
INSERT INTO t1 VALUES (2);
DROP TABLE t1;
FLUSH LOGS;
FLUSH LOGS;
include/sync_slave_io_with_master.inc
include/start_slave_sql.inc
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than 180 seconds.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET @@GLOBAL.DEBUG=@save_global_debug;
SET @@global.slave_transaction_retries=@saved_slave_transaction_retries;
include/rpl_end.inc
