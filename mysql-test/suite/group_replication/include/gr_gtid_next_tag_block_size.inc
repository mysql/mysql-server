# ==== PURPOSE ====
#
# This test is to validate execution of transactions with tagged GTIDs in
# topologies running with a Group Replication Plugin.
#
# ==== USAGE ====
#
# --let $use_block_size=100
# --source ../include/gr_gtid_next_tag_block_size.inc
#
# Parameters:
#
# $use_block_size
#   Chosen value for the group_replication_gtid_assignment_block_size
#
# ==== REQUIREMENTS ====
#
# R1. Multi-primary GR shall sustain a concurrent load of transactions with
#     automatically assigned GTIDs with specified tags.
#
# ==== IMPLEMENTATION ====
#
# This test is to validate execution of the
# SET GTID_NEXT=AUTOMATIC:<tag> command in the Group Replication plugin.
#
# T1.
#     Test steps:
#
#     1. Prepare one database for each server, create tables
#     2. Execute insert queries concurrently on 5 different servers,
#        using group name and two different tags for executed transactions
#        (GTID_NEXT='AUTOMATIC:<tag>')
#     3. Synchronize data in GR - wait for mysqlslap to finish executing queries
#     4. Verify that executed tagged GTIDs exist in the gtid_executed
#
#     Test pass conditions:
#
#     5. Pass condition 1: There should be N transactions with group UUID and
#        tag_1 on the primary
#     6. Pass condition 2: There should be N transactions with group UUID and
#        tag_2 on the primary
#     7. Pass condition 3: No errors or warnings in execution
#        (checked automatically)
#
# This test is to validate execution of the
# GTID_NEXT=<group_uuid>:<tag> in the Group Replication plugin.
#
# T2.
#     Test steps:
#
#     1. Execute transaction with assigned GTID using group name as an UUID
#        and defined tag (tag_1)
#     2. Execute transaction with AUTOMATIC GTID
#     3. Execute transaction with AUTOMATIC GTID and defined tag (tag_1)
#     4. Execute transaction with specified UUID and defined tag (tag_1)
#
#     Test pass conditions:
#
#     5. Pass condition 1: GTID_EXECUTED has expected value
#     6. Pass condition 2: No errors or warnings in execution
#        (checked automatically)
#
#     Note: Reused topology from T1
#
# ==== REFERENCES ====
#
# WL#15294 Extending GTID with tags to identify group of transactions
#          Step 4
#

--let $num_inserts = 50

--echo
--echo # T1. Group replication topology : 5 group members, multi-primary
--echo

--let $rpl_connection_name= server1
--source include/connection.inc
--let $block_size_saved= `SELECT @@GLOBAL.group_replication_gtid_assignment_block_size`
--eval SET GLOBAL group_replication_gtid_assignment_block_size=$use_block_size
--source include/start_and_bootstrap_group_replication.inc

--let $srv_id=2
while ($srv_id <= $rpl_server_count) {
  # Start GR on server$srv_num (Secondary $srv_num)
  --let $rpl_connection_name= server$srv_id
  --source include/connection.inc
  --eval SET GLOBAL group_replication_gtid_assignment_block_size=$use_block_size
  --source include/start_group_replication.inc
  --inc $srv_id
}

--echo
--echo # T1.1. Prepare databases and tables on server 1
--echo

--let $rpl_connection_name= server1
--source include/connection.inc

SET GTID_NEXT='AUTOMATIC:tag_1';
--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  --eval CREATE DATABASE test$srv_id
  --eval CREATE TABLE test$srv_id.t1 (c1 INT NOT NULL PRIMARY KEY AUTO_INCREMENT) ENGINE=InnoDB
  --inc $srv_id
}
SET GTID_NEXT='AUTOMATIC';

--echo
--echo # T1.2. Execute insert queries concurrently on 5 different servers,
--echo #       using group name and two different tags for executed transactions
--echo #       (GTID_NEXT='AUTOMATIC:<tag>')
--echo

--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  --let $rpl_connection_name= server$srv_id
  --source include/connection.inc
  --let $MYSQL_SVR_PORT= `SELECT @@port`

  --exec_in_background $MYSQL_SLAP --host=127.0.0.1 -P $MYSQL_SVR_PORT --create-schema=test$srv_id --delimiter=";" --iterations=$num_inserts --query="SET GTID_NEXT='AUTOMATIC:tag_1'; INSERT INTO t1 VALUES (NULL)" --concurrency=1 --silent
  --exec_in_background $MYSQL_SLAP --host=127.0.0.1 -P $MYSQL_SVR_PORT --create-schema=test$srv_id --delimiter=";" --iterations=$num_inserts --query="SET GTID_NEXT='AUTOMATIC:tag_2'; INSERT INTO t1 VALUES (NULL)" --concurrency=1 --silent
  --inc $srv_id
}

--echo
--echo # T1.3. Synchronize data in GR - wait for mysqlslap to finish
--echo #       executing queries
--echo

--let $rpl_connection_name= server1
--source include/connection.inc

--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  --let $wait_timeout= 300
  --let $wait_condition= SELECT count(*) = $num_inserts*2 FROM test$srv_id.t1
  --source include/wait_condition.inc
  --inc $srv_id
}

--source include/rpl/sync.inc

# Prepare gtid_executed string that gtid_executed set will be tested against for
# Test 1 and 2

--let $current_block_size = `SELECT @@GLOBAL.group_replication_gtid_assignment_block_size`
--let $srv_id=2
--let $counter=`SELECT $current_block_size + 1`
--let $intervals=
while ($srv_id <= $rpl_server_count) {
  --let $trx_num=50
  --let $int_end = `SELECT $counter + $trx_num - 1`
  --let $intervals = `SELECT CONCAT("$intervals", ":", "$counter", "-", "$int_end")`
  --let $counter = `SELECT $counter + $current_block_size`
  --inc $srv_id
}
--let $gtid_executed_target_t1 = `SELECT CONCAT("$group_replication_group_name:tag_1:1-60", "$intervals", ":tag_2:1-50", "$intervals")`
--let $gtid_executed_target_t2 = `SELECT CONCAT("11111111-1111-1111-1111-111111111111:tag_1:1\n,$group_replication_group_name:1-7:tag_1:1-61", "$intervals", ":4611686018427387904", ":tag_2:1-50", "$intervals")`

--echo
--echo # T1.4. Verify that executed tagged GTIDs exist in the gtid_executed
--echo

--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  if ($srv_id == 1) {
    --let $member_1_gtid_executed = `SELECT @@global.gtid_executed`
  }
  if ($srv_id != 1) {
    --let $assert_text = gtid_executed should be the same on member 1 and member $srv_id
    --let $assert_cond = "[SELECT @@global.gtid_executed]" = "$member_1_gtid_executed"
    --source include/assert.inc
  }

  --let $rpl_connection_name= server$srv_id
  --source include/connection.inc

  if (`SELECT @@GLOBAL.group_replication_gtid_assignment_block_size > 260`)
  {
    --let $assert_text= Ensure that group_replication_group_name transactions have executed correctly. T1, pass condition 2., T1, pass condition 3.
    --let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" = "$gtid_executed_target_t1"
    --source include/assert.inc
  }
  if (`SELECT @@GLOBAL.group_replication_gtid_assignment_block_size = 1`)
  {
    --let $assert_text= Ensure that group_replication_group_name transactions have executed correctly. T1, pass condition 2., T1, pass condition 3.
    --let $assert_cond= "[SELECT @@GLOBAL.GTID_EXECUTED]" = "$group_replication_group_name:tag_1:1-260:tag_2:1-250"
    --source include/assert.inc
  }
  --inc $srv_id
}

--echo
--echo # T1.5. Clean up
--echo

--let $rpl_connection_name= server1
--source include/connection.inc

--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  --eval DROP DATABASE test$srv_id
  --inc $srv_id
}

--echo
--echo # T2 Topology reused from T1
--echo

--let $rpl_connection_name= server1
--source include/connection.inc

CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY AUTO_INCREMENT) ENGINE=InnoDB;

--echo
--echo # T2.1. Execute transaction with assigned GTID using group name
--echo #       as an UUID and defined tag (tag_1)
--echo

--replace_result $group_replication_group_name GROUP_REPLICATION_GROUP_NAME
--eval SET GTID_NEXT='$group_replication_group_name:tag_1:4611686018427387904'
INSERT INTO t1 VALUES (NULL);

--echo
--echo # T2.2. Execute transaction with AUTOMATIC GTID
--echo

--eval SET GTID_NEXT='AUTOMATIC'
INSERT INTO t1 VALUES (NULL);

--echo
--echo # T2.3. Execute transaction with AUTOMATIC GTID and defined tag (tag_1)
--echo

--eval SET GTID_NEXT='AUTOMATIC:tag_1'
INSERT INTO t1 VALUES (NULL);

--echo
--echo # T2.4. Execute transaction with specifed GTID and defined tag (tag_1)
--echo

--eval SET GTID_NEXT='11111111-1111-1111-1111-111111111111:tag_1:1'
INSERT INTO t1 VALUES (NULL);
SET GTID_NEXT='AUTOMATIC';

--echo
--echo # T2.5. Check GTID_EXECUTED, T2 pass condition 1.
--echo

if (`SELECT @@GLOBAL.group_replication_gtid_assignment_block_size > 260`)
{
  --let $assert_text= Ensure that group_replication_group_name transactions have executed correctly. T1, pass condition 2., T1, pass condition 3.
  --let $assert_cond= GTID_IS_EQUAL("[SELECT @@GLOBAL.GTID_EXECUTED]", "$gtid_executed_target_t2")
  --source include/assert.inc
}
if (`SELECT @@GLOBAL.group_replication_gtid_assignment_block_size = 1`)
{
  --let $assert_text= Ensure that group_replication_group_name transactions have executed correctly. T1, pass condition 2., T1, pass condition 3.
  --let $assert_cond= GTID_IS_EQUAL("[SELECT @@GLOBAL.GTID_EXECUTED]", "11111111-1111-1111-1111-111111111111:tag_1:1,\n$group_replication_group_name:1-7:tag_1:1-261:4611686018427387904:tag_2:1-250")
  --source include/assert.inc
}
if (`SELECT @@GLOBAL.group_replication_gtid_assignment_block_size > 1 AND @@GLOBAL.group_replication_gtid_assignment_block_size <= 260`)
{
  --let $assert_text= Ensure that gtid_executed contains expected number of transactions. T1, pass condition 2., T1, pass condition 3.
  --let $assert_cond= GTID_COUNT("[SELECT @@GLOBAL.GTID_EXECUTED]") = 520
  --source include/assert.inc
}

--echo
--echo # Clean up after T2
--echo

DROP TABLE t1;

--let $srv_id=1
while ($srv_id <= $rpl_server_count) {
  --let $rpl_connection_name= server$srv_id
  --source include/connection.inc
  --source include/stop_group_replication.inc
  --disable_query_log
  --eval SET GLOBAL group_replication_gtid_assignment_block_size=$block_size_saved
  --enable_query_log
  RESET BINARY LOGS AND GTIDS;
  --inc $srv_id
}

--let $rpl_connection_name= server1
--source include/connection.inc
