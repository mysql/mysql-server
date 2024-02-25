################################################################################
# Validate that BEFORE, AFTER and BEFORE_AND_AFTER consistency levels do
# protect against outdated reads when a new primary is still applying the
# backlog.
#
# Test:
#  0. The test requires two servers: M1 and M2.
#  1. Create a table on the group.
#  2. Join server2 to group and lock table t1 to block the
#     the backlog apply on server2 once it is elected the
#     primary.
#  3. Execute a transaction on the primary server (server1),
#     server2 will be unable to apply it since table t1 is
#     locked.
#  4. Validate that server2 did certify but did not apply the
#     transaction.
#  5. Switch primary to server2.
#  6. Validate that primary election is executing
#  7. A transaction with consistency level BEFORE, AFTER and
#     BEFORE_AND_AFTER on the new primary will be put on hold
#     until the backlog is applied.
#  8. Validate transaction is on hold
#  9. UNLOCK tables to allow backlog to be applied on server2
# 10. The hold SELECT will now be done and will return the
#     correct value 1.
# 11. Reap with success the primary switch.
# 12. Cleanup
################################################################################
#
# ==== Usage ====
#
# --let $transaction_T1_consistency= ...
#
#
# Parameters:
#
#   $transaction_T1_consistency
#     Set the given consistency guarantee to transaction T1.
#
################################################################################

if (!$transaction_T1_consistency)
{
  --die "Missing argument 'transaction_T1_consistency'"
}

--source include/have_group_replication_plugin.inc
--let $rpl_skip_group_replication_start= 1
--let $rpl_group_replication_single_primary_mode=1
--source include/group_replication.inc

--echo
--echo ############################################################
--echo # 1. Create a table on the group.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--source include/start_and_bootstrap_group_replication.inc
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY);


--echo
--echo ############################################################
--echo # 2. Join server2 to group and lock table t1 to block the
--echo #    the backlog apply on server2 once it is elected the
--echo #    primary.
--let $rpl_connection_name= server_2
--source include/rpl_connection.inc
--let $member2_uuid= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1)

--source include/start_group_replication.inc
LOCK TABLES t1 READ;


--echo
--echo ############################################################
--echo # 3. Execute a transaction on the primary server (server1),
--echo #    server2 will be unable to apply it since table t1 is
--echo #    locked.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
INSERT INTO t1 VALUES (1);


--echo
--echo ############################################################
--echo # 4. Validate that server2 did certify but did not apply the
--echo #    transaction.
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*) = 1 FROM performance_schema.replication_group_member_stats WHERE member_id='$member2_uuid' AND count_transactions_checked = 1 AND count_transactions_remote_in_applier_queue = 1
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 5. Switch primary to server2.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--replace_result $member2_uuid MEMBER2_UUID
--send_eval SELECT group_replication_set_as_primary("$member2_uuid")


--echo
--echo ############################################################
--echo # 6. Validate that primary election is executing
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--let $wait_condition=SELECT COUNT(*)=1 FROM performance_schema.events_stages_current WHERE event_name LIKE "%Primary Switch: executing Primary election%"
--source include/wait_condition.inc

--let $wait_condition=SELECT COUNT(*)=1 FROM performance_schema.events_stages_current WHERE event_name LIKE "%Primary Election: applying buffered transactions%"
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 7. A transaction with consistency level BEFORE, AFTER and
--echo #    BEFORE_AND_AFTER on the new primary will be put on hold
--echo #    until the backlog is applied.
--eval SET @@SESSION.group_replication_consistency= '$transaction_T1_consistency'
--send SELECT COUNT(*) FROM t1


--echo
--echo ############################################################
--echo # 8. Validate transaction is on hold
--let $rpl_connection_name= server_2
--source include/rpl_connection.inc
--let $wait_condition= SELECT COUNT(*)=1 FROM INFORMATION_SCHEMA.PROCESSLIST WHERE STATE='Executing hook on transaction begin.' AND INFO='SELECT COUNT(*) FROM t1'
--source include/wait_condition.inc


--echo
--echo ############################################################
--echo # 9. UNLOCK tables to allow backlog to be applied on server2
--let $rpl_connection_name= server_2
--source include/rpl_connection.inc
UNLOCK TABLES;


--echo
--echo ############################################################
--echo # 10. The hold SELECT will now be done and will return the
--echo #     correct value 1.
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--reap
SET @@SESSION.group_replication_consistency= DEFAULT;


--echo
--echo ############################################################
--echo # 11. Reap with success the primary switch.
--let $rpl_connection_name= server1
--source include/rpl_connection.inc
--replace_result $member2_uuid MEMBER2_UUID
--reap


--echo
--echo ############################################################
--echo # 12. Cleanup
--let $rpl_connection_name= server2
--source include/rpl_connection.inc
--let $wait_condition= SELECT @@GLOBAL.read_only = 0
--source include/wait_condition_or_abort.inc
DROP TABLE t1;

--source include/group_replication_end.inc
