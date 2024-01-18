# ==== Purpose ====
#
# This include file is a parametric test that does following:
# 1) Configure two servers (server1, server2) to be in group replication
# 2) Verifies group_replication_role on both servers
# 3) create a table on primary node
# 4) tries to insert data into the table, depending on server1 configuration
#    this operation could fail
#
# ==== Usage ====
#
#   --let $test_parameter_enforce_checks= TRUE|FALSE
#   --let $test_parameter_single_primary_mode= TRUE|FALSE
#   --let $test_parameter_tx_isolation= READ-UNCOMMITTED|READ-COMMITTED|REPEATABLE-READ|SERIALIZABLE
#   --let $test_parameter_sql_execution_error= N|ERROR_NAME
#   --let $test_parameter_primary_uuid= UUID
#   --source ../include/gr_single_primary_with_transaction_isolation.inc
#
# Parameters:
#   $test_parameter_enforce_checks
#     Set value to group_replication_enforce_update_everywhere_checks system variable
#
#   $test_parameter_single_primary_mode
#     Set value to group_replication_single_primary_mode system variable
#
#   $test_parameter_tx_isolation
#     Set value to transaction_isolation session system variable
#
#   $test_parameter_sql_execution_error
#     Should the test expect an error while executing INSERT statement. If not
#     the test is going to perform additional check to verify data consistency
#     between server1 and server2
#
#   $test_parameter_primary_uuid
#     Test verifies if the primary member has this uuid
#

--let $include_filename= gr_single_primary_with_transaction_isolation.inc
--source include/begin_include_file.inc

--let $_saved_connection= $CURRENT_CONNECTION

--echo # CONFIGURE SERVER 1
--let $rpl_connection_name= server1
--source include/connection.inc
--eval SET GLOBAL group_replication_enforce_update_everywhere_checks= $test_parameter_enforce_checks
--eval SET GLOBAL group_replication_single_primary_mode= $test_parameter_single_primary_mode
--eval SET SESSION transaction_isolation=$test_parameter_tx_isolation

--source include/start_and_bootstrap_group_replication.inc

if ($test_parameter_single_primary_mode == TRUE)
{
  --let $group_replication_expected_uuid= $test_parameter_primary_uuid
  --source include/gr_wait_primary_member_uuid.inc
}

if ($test_parameter_single_primary_mode == FALSE)
{
  #if multi-primary the members ONLINE in the group all should be primary
  --let $_gr_wait_primary_member_uuid_online_count = query_get_value(SELECT COUNT(*) COUNT FROM performance_schema.replication_group_members WHERE member_state="ONLINE", COUNT, 1)
  --let $wait_condition= SELECT COUNT(*)=$_gr_wait_primary_member_uuid_online_count FROM performance_schema.replication_group_members WHERE MEMBER_ROLE="PRIMARY"
  --source include/wait_condition_or_abort.inc
}


--echo # CONFIGURE SERVER 2
--let $rpl_connection_name= server2
--source include/connection.inc
--eval SET GLOBAL group_replication_enforce_update_everywhere_checks= $test_parameter_enforce_checks
--eval SET GLOBAL group_replication_single_primary_mode= $test_parameter_single_primary_mode
--eval SET SESSION transaction_isolation=$test_parameter_tx_isolation
--source include/start_group_replication.inc

if ($test_parameter_single_primary_mode == TRUE)
{
  --let $group_replication_expected_uuid= $test_parameter_primary_uuid
  --source include/gr_wait_primary_member_uuid.inc
}

if ($test_parameter_single_primary_mode == FALSE)
{
  #if multi-primary the members ONLINE in the group all should be primary
  --let $_gr_wait_primary_member_uuid_online_count = query_get_value(SELECT COUNT(*) COUNT FROM performance_schema.replication_group_members WHERE member_state="ONLINE", COUNT, 1)
  --let $wait_condition= SELECT COUNT(*)=$_gr_wait_primary_member_uuid_online_count FROM performance_schema.replication_group_members WHERE MEMBER_ROLE="PRIMARY"
  --source include/wait_condition_or_abort.inc
}

--echo # WRITE DATA ON SERVER 1 (ISOLATION LEVEL $test_parameter_tx_isolation)
--echo # AND VERIFY PRESENCE OF THE DATA ON SERVER 2
--let $rpl_connection_name= server1
--source include/connection.inc
CREATE TABLE first(c1 INT NOT NULL AUTO_INCREMENT PRIMARY KEY);

--error $test_parameter_sql_execution_error
INSERT INTO first(c1) VALUES(2),(4);
if ($test_parameter_sql_execution_error == 0)
{
  --source include/rpl/sync.inc
  --let $rpl_connection_name= server2
  --source include/connection.inc

  --let $assert_text= All data inserted on server1 should be present on server2
  --let $assert_cond= [SELECT COUNT(*) as count FROM first, count, 1] = 2
  --source include/assert.inc

  SELECT * FROM first;
}

--let $rpl_connection_name= server1
--source include/connection.inc
DROP TABLE first;

--let $rpl_connection_name= server2
--source include/connection.inc
--source include/stop_group_replication.inc

--let $rpl_connection_name= server1
--source include/connection.inc
--source include/stop_group_replication.inc

--connection $_saved_connection

--let $test_parameter_primary_uuid=

--let $include_filename= gr_single_primary_with_transaction_isolation.inc
--source include/end_include_file.inc
