include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
call mtr.add_suppression("Unsafe statement written to the binary log using statement format");
call mtr.add_suppression("Unsafe statement written to the binary log using statement format");
[on master]
CREATE TABLE t1 (a VARCHAR(100), b INT);
INSERT INTO t1 VALUES ("zero", 0);
==== Normal setup ====
[on slave]
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/start_slave.inc
include/assert.inc [SHOW SLAVE STATUS should return the same delay that we set with CHANGE MASTER]
[on master]
INSERT INTO t1 VALUES ('normal setup', 1);
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
==== Slave lags "naturally" after master ====
[on master]
# CREATE FUNCTION delay_on_slave(time_units INT) RETURNS INT BEGIN IF @@server_id = 2 THEN RETURN SLEEP(time_units * T); ELSE RETURN 0; END IF; END
INSERT INTO t1 SELECT delay_on_slave(3), 2;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
INSERT INTO t1 VALUES ('slave is already lagging: this statement should execute immediately', 3);
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
INSERT INTO t1 SELECT delay_on_slave(2), 4;
Warnings:
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system variable that may have a different value on the slave.
Note	1592	Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT. Statement is unsafe because it uses a system function that may return a different value on the slave.
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
==== STOP SLAVE / START SLAVE + DML ====
[connection slave]
include/stop_slave.inc
include/start_slave.inc
[on master]
INSERT INTO t1 VALUES ('stop slave and start slave: DML', 5);
[on slave]
include/stop_slave.inc
include/assert.inc [SQL thread position should not increase after STOP SLAVE]
include/assert.inc [Query should not be executed after STOP SLAVE]
include/assert.inc [Status should be '' after STOP SLAVE]
include/start_slave.inc
[connection master]
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
==== STOP SLAVE / START SLAVE + DDL ====
This verifies BUG#56442
[on master]
CREATE TABLE t_check_dml_not_executed_prematurely (a INT);
include/save_master_pos.inc
[on slave]
include/stop_slave.inc
include/assert.inc [SQL thread position should not increase after STOP SLAVE]
include/assert.inc [Query should not be executed after STOP SLAVE]
include/assert.inc [Status should be '' after STOP SLAVE]
include/start_slave.inc
[connection master]
[connection slave]
include/assert.inc [Assert that the REMAINING_DELAY from performance_schema is same as SQL_Remaining_Delay in the output of show slave status.]
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [Status should not be 'Waiting until SOURCE_DELAY seconds after source executed event']
[connection master]
[connection slave]
include/assert.inc [The difference between the immediate_commit_timestamp should be at least the SQL_Delay]
[connection master]
[connection slave]
include/assert.inc [DDL Query should be executed]
include/check_slave_is_running.inc
==== Change back to no delay ====
[on slave]
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = 0;
include/assert.inc [Delay should be 0 when we set it to 0]
==== Reset delay with RESET SLAVE ====
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = 71;
include/start_slave.inc
include/assert.inc [Delay should be 71 when we set it to 71]
include/stop_slave.inc
RESET SLAVE;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
include/assert.inc [Delay should be 0 after RESET SLAVE]
==== Set an invalid value for the delay ====
include/stop_slave.inc
# Expect error for setting negative delay
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = -1;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '-1' at line 1
# Expect that it's ok to set delay of 2^31-1
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = 2147483647;
# Expect error for setting delay between 2^31 and 2^32-1
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = 2147483648;
ERROR HY000: The requested value 2147483648 for the master delay exceeds the maximum 2147483647
# Expect error for setting delay to nonsense
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = blah;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'blah' at line 1
CHANGE REPLICATION SOURCE TO SOURCE_DELAY = 0;
include/start_slave.inc
==== Clean up ====
[on master]
DROP TABLE t1, t_check_dml_not_executed_prematurely;
DROP FUNCTION delay_on_slave;
[on slave]
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
