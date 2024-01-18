# ==== Purpose ====
#
# Validate group_replication_consistency= 'AFTER' behaviour.
#
# Test:
# 00. The test requires two servers: M1 and M2.
# 01. Create two tables on the group.
# 02. Lock table t1 on server2 to block a future update.
# 03. Execute transaction T1, the transaction will block since
#     server2 cannot prepare.
# 04. Validate transactions status.
# 05. Since T1 is not yet prepared on server2, new
#     transactions are allowed.
# 06. Force server2 to block between T1 prepare and commit
#     and unlock table t1.
# 07. Now that T1 is prepared on server2, server1 COMMIT is
#     unblocked.
# 08. Now that T1 is prepared on server2 but it is not yet
#     committed, new transactions are held.
# 09. Unblock T1 prepare on server2.
# 10. Clean up.
################################################################################

#
# ==== Usage ====
#
# --let $set_gtid_next= UUID:GNO
#
#
# Parameters:
#
#   $set_gtid_next
#     Set the given GTID on transaction T1.
#     We have two code paths on the certification code:
#      1) GTID generated automatically;
#      2) GTID assigned by SET GTID_NEXT.
#     The parameter $set_gtid_next allow us to instrument both paths.
#
################################################################################
--source include/have_debug_sync.inc
--source include/not_have_privilege_checks_user.inc
--source include/have_group_replication_plugin.inc
--source include/group_replication.inc

--echo
--echo ############################################################
--echo # 01. Create two tables on the group.
--let $rpl_connection_name= server1
--source include/connection.inc
# Assert consistency options metrics.
--let $assert_text= 'There were 0 transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc
--let $assert_text= 'There was no wait time on transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_time_sum", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

SET @@SESSION.group_replication_consistency= 'AFTER';
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY);
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY);

# Assert consistency options metrics.
--let $assert_text= 'There were 2 transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_count", VARIABLE_VALUE, 1] = 2
--source include/assert.inc

--let $assert_text= 'There was wait time on transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_time_sum", VARIABLE_VALUE, 1] > 0
--source include/assert.inc
--let $_consistency_after_termination_time_sum_02= `SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_time_sum"`

--let $assert_text= 'There were 2 data messages sent'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = 2
--source include/assert.inc


--echo
--echo ############################################################
--echo # 02. Lock table t1 on server2 to block a future update.
--let $rpl_connection_name= server_2
--source include/connection.inc
LOCK table t1 READ;


--echo
--echo ############################################################
--echo # 03. Execute transaction T1, the transaction will block since
--echo #     server2 cannot prepare.
--let $rpl_connection_name= server1
--source include/connection.inc
if ($set_gtid_next)
{
  --eval SET GTID_NEXT= "$set_gtid_next"
}
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
--send COMMIT


--echo
--echo ############################################################
--echo # 04. Validate transactions status.
# server1 is waiting for commit
--let $rpl_connection_name= server_1
--source include/connection.inc
--let $wait_condition=SELECT COUNT(*)=1 FROM information_schema.processlist WHERE user='root' AND state='waiting for handler commit' AND info='COMMIT'
--source include/wait_condition.inc

# server2 is waiting for the locked table
--let $rpl_connection_name= server2
--source include/connection.inc
--let $wait_condition=SELECT COUNT(*)=1 FROM information_schema.processlist WHERE user='system user' AND state='Waiting for table metadata lock'
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 05. Since T1 is not yet prepared on server2, new
--echo #     transactions are allowed.
--let $rpl_connection_name= server2
--source include/connection.inc
--let $assert_text= 'There are no values in table t2'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t2, count, 1] = 0
--source include/assert.inc


--echo
--echo ############################################################
--echo # 06. Force server2 to block between T1 prepare and commit
--echo #     and unlock table t1.
--let $rpl_connection_name= server_2_1
--source include/connection.inc
SET @@GLOBAL.DEBUG= '+d,group_replication_wait_on_after_applier_prepare';

--let $rpl_connection_name= server_2
--source include/connection.inc
UNLOCK TABLES;

--let $rpl_connection_name= server_2_1
--source include/connection.inc
# Wait for the debug sync to be reached.
SET DEBUG_SYNC= "now WAIT_FOR signal.after_applier_prepare_waiting";


--echo
--echo ############################################################
--echo # 07. Now that T1 is prepared on server2, server1 COMMIT is
--echo #     unblocked.
--let $rpl_connection_name= server1
--source include/connection.inc
--reap

if ($set_gtid_next)
{
  SET GTID_NEXT= 'AUTOMATIC';
}

--let $assert_text= 'There is 1 value in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE c1=1, count, 1] = 1
--source include/assert.inc

--let $assert_text= 'There is 1 value in table t2'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t2 WHERE c1=1, count, 1] = 1
--source include/assert.inc

# Assert consistency options metrics.
--let $assert_text= 'There were 3 transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_count", VARIABLE_VALUE, 1] = 3
--source include/assert.inc

--let $assert_text= 'There was wait time on transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_time_sum", VARIABLE_VALUE, 1] > $_consistency_after_termination_time_sum_02
--source include/assert.inc

--let $assert_text= 'There were 3 data messages sent'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = 3
--source include/assert.inc


--echo
--echo ############################################################
--echo # 08. Now that T1 is prepared on server2 but it is not yet
--echo #     committed, new transactions are held.
--let $rpl_connection_name= server_2
--source include/connection.inc
# Assert consistency options metrics.
--let $assert_text= 'There were 0 transactions that waited for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

--let $assert_text= 'There was no wait time for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_time_sum", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

--let $assert_text= 'There were no data messages sent'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

--send SELECT COUNT(*) AS count FROM t2 WHERE c1=1

# server_2_1 is waiting for T1 to commit
--let $rpl_connection_name= server2
--source include/connection.inc
--let $wait_condition=SELECT COUNT(*)=1 FROM information_schema.processlist WHERE user='root' AND state='Executing hook on transaction begin.' AND info="SELECT COUNT(*) AS count FROM t2 WHERE c1=1"
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 09. Unblock T1 prepare on server2.
--let $rpl_connection_name= server_2_1
--source include/connection.inc
# Signal debug sync to continue.
SET DEBUG_SYNC= 'now SIGNAL signal.after_applier_prepare_continue';
SET @@GLOBAL.DEBUG= '-d,group_replication_wait_on_after_applier_prepare';

--let $rpl_connection_name= server_2
--source include/connection.inc
--reap

--let $assert_text= 'There is 1 value in table t1'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t1 WHERE c1=1, count, 1] = 1
--source include/assert.inc

--let $assert_text= 'There is 1 value in table t2'
--let $assert_cond= [SELECT COUNT(*) AS count FROM t2 WHERE c1=1, count, 1] = 1
--source include/assert.inc

# Assert consistency options metrics.
--let $assert_text= 'There was 1 transaction that waited for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_count", VARIABLE_VALUE, 1] = 1
--source include/assert.inc

--let $assert_text= 'There was wait time for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_time_sum", VARIABLE_VALUE, 1] > 0
--source include/assert.inc

--let $assert_text= 'There were 0 transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

--let $assert_text= 'There was no wait time on transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_termination_time_sum", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

--let $assert_text= 'There were no data messages sent'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_data_messages_sent_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc


--echo
--echo ############################################################
--echo # 10. Clean up.
--let $rpl_connection_name= server1
--source include/connection.inc
# Assert consistency options metrics.
--let $assert_text= 'There were 0 transactions that waited for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_count", VARIABLE_VALUE, 1] = 0
--source include/assert.inc
--let $assert_text= 'There was no wait time for transactions with group_replication_consistency=AFTER'
--let $assert_cond= [SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME="Gr_transactions_consistency_after_sync_time_sum", VARIABLE_VALUE, 1] = 0
--source include/assert.inc

DROP TABLE t1;
DROP TABLE t2;
SET @@SESSION.group_replication_consistency= DEFAULT;

--source include/group_replication_end.inc
