# ==== Purpose ====
#
# Changes replication topology. This file is normally sourced from
# include/rpl/init.inc, but test cases can also source it if they
# need to change topology after they have sourced include/rpl/init.inc
#
# This file sets up variables needed by include/rpl/sync.inc and many
# other replication scripts in the include/ directory.  It also issues
# CHANGE REPLICATION SOURCE on all servers where the configuration changes from
# what it was before.  It does not issue START REPLICA (use
# include/rpl/start_all_replicas.inc for that).
#
# Note: it is not currently possible to change the number of servers
# after the rpl/init.inc, without first calling rpl/deinit.inc. So the
# test has to set $rpl_server_count to the total number of servers
# that the test uses, before it sources include/rpl/init.inc.  After
# that, $rpl_server_count must not change until after next time the
# test sources include/rpl/deinit.inc.
#
# Note: Since this script issues CHANGE REPLICATION SOURCE, the test case must
# ensure that all slaves where the configuration changes have stopped
# both the IO thread and the SQL thread before this script is sourced.
#
#
# ==== Usage ====
#
# [--let $rpl_server_count= 7]
# --let $rpl_topology= 1->2->3->1->4, 2->5, 6->7
# [--let $rpl_multi_source= 1]
# [--let $use_gtids= 1]
# [--let $rpl_group_replication= 1]
# [--let $rpl_skip_change_master= 1]
# [--let $rpl_source_log_file= 1:master-bin.000001,3:master-bin.000003]
# [--let $rpl_source_log_pos= 1:4711,3:107]
# [--let $rpl_debug= 1]
# [--let $rpl_unconditional_change_master= 1]
# [--let $rpl_privilege_checks_user= *:'user'@'host',1:'user'@'host',2:NULL]
# [--let $rpl_privilege_checks_user_dont_create_user= 0]
# [--let $rpl_privilege_checks_user_additional_grants= CREATE,INSERT,SELECT]
# [--let $rpl_require_row_format= *:0,1:0,2:1]
# [--let $rpl_gtid_only= *:0,1:0,2:1]
# --source include/rpl/change_topology.inc
#
# Parameters:
#   $use_gtids
#     Use option SOURCE_AUTO_POSITION = 1 to CHANGE REPLICATION SOURCE.
#
#   $rpl_source_log_file
#     By default, CHANGE REPLICATION SOURCE is executed without specifying the
#     SOURCE_LOG_FILE parameter. This variable can be set to specify a
#     filename.  This variable should be a comma-separated list of the
#     following form:
#
#       SERVER_NUMBER_1:FILE_NAME_1,SERVER_NUMBER_2:FILE_NAME_2,...
#
#     Before CHANGE REPLICATION SOURCE is executed on server N, this script checks
#     if $rpl_source_log_file contains the text N:FILE_NAME. If it
#     does, then SOURCE_LOG_FILE is set to FILE_NAME. Otherwise,
#     SOURCE_LOG_FILE is not specified.  For example, to specify that
#     server_1 should start replicate from master-bin.000007 and
#     server_5 should start replicate from master-bin.012345, do:
#
#       --let $rpl_source_log_file= 1:master-bin.000007,5:master-bin.012345
#
#   $rpl_source_log_pos
#     By default, CHANGE REPLICATION SOURCE is executed without specifying the
#     SOURCE_LOG_POS parameter.  This variable can be set to set a
#     specific position.  It has the same form as $rpl_source_log_file
#     (see above).  For example, to specify that server_3 should start
#     replicate from position 4711 of its master, do:
#       --let $rpl_source_log_pos= 3:4711
#
#   $rpl_unconditional_change_master
#     This script remembers the topology set by previous invokations
#     of either rpl/init.inc or of this script.  By default, this
#     script only executes CHANGE REPLICATION SOURCE on servers that
#     actually have a new master.  If $rpl_unconditiona_change_master
#     is set, then this script unconditionally executes
#     CHANGE REPLICATION SOURCE on all servers that have a master.  This
#     is useful either if you have changed the topology without invoking
#     the scripts or if you want to set some other parameters of CHANGE
#     REPLICATION SOURCE.
#
#   $rpl_server_count, $rpl_topology, $rpl_debug,
#   $rpl_skip_change_master, $rpl_multi_source
#     See include/rpl/init.inc
#
#   $rpl_group_replication
#     This checks for whether we have Group Replication enabled or not. Based
#     on the check this parameter we execute CHANGE REPLICATION SOURCE or
#     not in this inc file.
#
#   $rpl_privilege_checks_user
#     By default, CHANGE REPLICATION SOURCE is executed without specifying the
#     `PRIVILEGE_CHECKS_USER` parameter. Instantiating
#     `$rpl_privilege_checks_user` sets the `PRIVILEGE_CHECKS_USER` option for
#     the `CHANGE REPLICATION SOURCE TO...` command, bounding the replication applier to
#     execute within the security context of the given user. For example, to
#     specify that server_1 should use 'u1'@'localhost' as base user for
#     replication applier security context and server_5 shouldn't check
#     privileges while applying replicated events, do:
#
#       --let $rpl_privilege_checks_user= 1:'u1'@'localhost',5:NULL
#
#     To specify that all node applier thread should run with a given user's
#     security context, do:
#
#       --let $rpl_privilege_checks_user= *:'u1'@'localhost'
#
#   $rpl_privilege_checks_user_dont_create_user
#     Force to not create the user provided in `$rpl_privilege_checks_user`. If
#     set to 0, creates it if doesn't exists and, if created, assigns the
#     `REPLICATION_APPLIER` privilege.
#
#   $rpl_privilege_checks_user_additional_grants
#     Specify additional privileges to be granted to the
#     $rpl_privilege_checks_user user.
#
#   $rpl_require_row_format
#     Include `REQUIRE_ROW_FORMAT` option in the `CHANGE REPLICATION SOURCE TO ...`
#     statement to be executed. For example, to specify that server_1 should
#     have the flag enabled and server_5 shouldn't, do:
#
#       --let $rpl_require_row_format= 1:1,5:0
#
#     To specify that all nodes have the flag enabled, do:
#
#       --let $rpl_require_row_format= *:1
#
#   $rpl_require_table_primary_key_check
#     By default, CHANGE REPLICATION SOURCE is executed without specifying the
#     `REQUIRE_TABLE_PRIMARY_KEY_CHECK` parameter.
#     Instantiating `$rpl_require_table_primary_key_check` sets
#     `REQUIRE_TABLE_PRIMARY_KEY_CHECK` for the `CHANGE REPLICATION SOURCE TO...` command.
#     For example, to specify that server_1 should use STREAM as its policy
#     for primary keys and server_5 shouldn't do the checks do:
#
#       --let $rpl_require_table_primary_key_check= 1:STREAM,5:OFF
#
#     To specify that all servers should have the same check policy, do:
#
#       --let $rpl_require_table_primary_key_check= *:ON
#
#   $rpl_gtid_only
#     Include `GTID_ONLY` option in the `CHANGE REPLICATION SOURCE TO ...`
#     statement to be executed. For example, to specify that server_1 should
#     have the flag enabled and server_5 shouldn't, do:
#
#       --let $rpl_gtid_only= 1:1,5:0
#
#     To specify that all nodes have the flag enabled, do:
#
#       --let $rpl_gtid_only= *:1

#
# ==== Internal variables configured by this file ====
#
# This file sets up the following variables, which are used by other
# low-level replication files such as:
#   include/rpl/sync.inc
#   include/rpl/start_all_replicas.inc
#   include/rpl/stop_all_replicas.inc
#   include/rpl/deinit.inc
#
# $rpl_connection_list
#   This variable will be set to a string consisting of N source-replica
#   specifications, where N is the number of source-replica
#   connections. Each source-replica specification has the form
#   'm<NUMBER1> s<NUMBER2> '. This indicates that server <NUMBER1> is a
#   direct master of server <NUMBER2>. Each <NUMBER> is padded to the
#   right by spaces to a fixed width of $rpl_server_count_length.  For
#   example, if $rpl_topology is '1->2->3->1,2->4,5->6', then
#   $rpl_connection_list is 'm1 s2 m2 s3 m3 s1 m2 s4 m5 s6'.
#   The length of this variable is exactly
#   2*(2+$rpl_connection_count_length)*$rpl_connection_list.
#
# $rpl_sync_chain_dirty
#   This variable is set to 1.  This tells include/rpl/sync.inc to
#   compute a new value for $rpl_sync_chain next time that
#   include/rpl/sync.inc is sourced.  See
#   include/rpl/impl/generate_sync_chain.inc and include/rpl/sync.inc for
#   details.
#
# See also rpl/init.inc


# Remove whitespace from $rpl_topology
--let $rpl_topology= `SELECT REPLACE('$rpl_topology', ' ', '')`

--let $include_filename= rpl/change_topology.inc [new topology=$rpl_topology]
--source include/begin_include_file.inc


if ($rpl_debug)
{
  --echo ---- Check input ----
}

# $rpl_server_count must not change between rpl/init.inc and rpl/deinit.inc
if ($rpl_server_count != $_rpl_server_count)
{
  --die ERROR IN TEST: Detected that $rpl_server_count has changed. You are not allowed to change this between rpl/init.inc and rpl/deinit.inc.
}

# $rpl_topology must be set
if ($rpl_topology == '')
{
  --die ERROR IN TEST: You must set $rpl_topology before you source include/rpl/change_topology.inc. If you really want to change to the empty topology, set $rpl_topology= none
}
--let $_rpl_topology= $rpl_topology
if ($_rpl_topology == 'none')
{
  --let $_rpl_topology=
}

# include/rpl/init.inc should be invoked before this file
if (!$rpl_inited)
{
  --die ERROR IN TEST: You must source include/rpl/init.inc before you source include/rpl/change_topology.inc
}

--let $_rpl_old_connection_list= $rpl_connection_list
if ($rpl_debug)
{
  --echo \$rpl_server_count='$rpl_server_count'
  --echo \$rpl_server_count_length='$rpl_server_count_length'
  --echo old \$rpl_connection_list='$rpl_connection_list'
  --echo old \$rpl_sync_chain='$rpl_sync_chain'
  --echo new \$rpl_topology='$_rpl_topology'
}

if ($rpl_debug)
{
  --echo ---- Generate \$rpl_connection_list and \$rpl_connection_count ----
}

--let $rpl_connection_list=
--let $rpl_connection_count= 0
while ($_rpl_topology)
{
  # Get 's1->s2' from 's1->s2->s3->...' or from 's1->s2,s3->s4,...'
  --let $_rpl_master_slave= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX('$_rpl_topology', ',', 1), '->', 2)`
  # Modify $_rpl_topology as follows:
  #  - If it starts with 's1->s2,', remove 's1->s2,'
  #  - If it starts with 's1->s2->', remove 's1->'
  #  - If it is equal to 's1->s2', remove 's1->s2'
  --let $_rpl_topology= `SELECT SUBSTR('$_rpl_topology', IF(SUBSTR('$_rpl_topology', LENGTH('$_rpl_master_slave') + 1, 2) != '->', LENGTH('$_rpl_master_slave'), LOCATE('->', '$_rpl_master_slave')) + 2)`
  # Get 's1' from 's1->s2'
  --let $_rpl_master= `SELECT RPAD(SUBSTRING_INDEX('$_rpl_master_slave', '->', 1), $rpl_server_count_length, ' ')`
  # Get 's2' from 's1->s2'
  --let $_rpl_slave= `SELECT RPAD(SUBSTRING('$_rpl_master_slave', LENGTH('$_rpl_master') + 3), $rpl_server_count_length, ' ')`

  if (!$rpl_multi_source)
  {
   # Check that s2 does not have another master.
    if (`SELECT LOCATE('s$_rpl_slave ', '$rpl_connection_list') != 0`)
    {
      --echo ERROR IN TEST: Server '$_rpl_slave' has more than one master in topology '$rpl_topology'
      --die ERROR IN TEST: found a server with more than one master in the $rpl_topology variable
    }
  }
  # Append 'm<s1> s<s2> ' to $rpl_connection_list
  --let $_rpl_new_connection= m$_rpl_master s$_rpl_slave$_rpl_space
  --let $rpl_connection_list= $rpl_connection_list$_rpl_new_connection
  --inc $rpl_connection_count
}

if ($rpl_debug)
{
  --echo new \$rpl_connection_list = '$rpl_connection_list'
  --echo new \$rpl_connection_count = '$rpl_connection_count'
}


if ($rpl_privilege_checks_user == ''){
  --let $rpl_privilege_checks_user = $PRIVILEGE_CHECKS_USER
}

--let $_rpl_change_topology_privilege_checks_all_index= `SELECT LOCATE('*:', "$rpl_privilege_checks_user")`
if ($_rpl_change_topology_privilege_checks_all_index)
{
  --let $_rpl_change_topology_privilege_checks_all= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_privilege_checks_user", $_rpl_change_topology_privilege_checks_all_index), ',', 1), ':', -1)`
}

--source include/rpl/impl/gtid_only_configuration.inc

--let $_rpl_change_topology_gtid_only_all_index= `SELECT LOCATE('*:', "$rpl_gtid_only")`
if ($_rpl_change_topology_gtid_only_all_index)
{
  --let $_rpl_change_topology_gtid_only_all= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_gtid_only", $_rpl_change_topology_gtid_only_all_index), ',', 1), ':', -1)`
}

# In order to enable GTID_ONLY, REQUIRE_ROW must be enabled

if ($rpl_gtid_only)
{
  --let $rpl_topology_override_variable = $rpl_require_row_format
  --let $rpl_topology_with_variable = $rpl_gtid_only
  --let $rpl_topology_when_override_value = 1
  --source include/rpl/impl/change_topology_merge_variables.inc
  --let $rpl_require_row_format = $rpl_topology_overriden_result
}

--let $_rpl_change_topology_require_row_all_index= `SELECT LOCATE('*:', "$rpl_require_row_format")`
if ($_rpl_change_topology_require_row_all_index)
{
  --let $_rpl_change_topology_require_row_all= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_require_row_format", $_rpl_change_topology_require_row_all_index), ',', 1), ':', -1)`
}

--let $_rpl_require_table_primary_key_check_all_index= `SELECT LOCATE('*:', "$rpl_require_table_primary_key_check")`
if ($_rpl_require_table_primary_key_check_all_index)
{
  --let $_rpl_require_table_primary_key_check_all= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_require_table_primary_key_check", $_rpl_require_table_primary_key_check_all_index), ',', 1), ':', -1)`
}

if (!$rpl_skip_change_master)
{
  if ($rpl_debug)
  {
    --echo ---- Execute CHANGE REPLICATION SOURCE on all servers ----
  }

  if (!$rpl_debug)
  {
    --disable_query_log
  }

  --let $rpl_source_file= include/rpl/impl/change_topology_helper.inc
  --source include/rpl/for_each_connection.inc
}


--let $rpl_sync_chain_dirty= 1
--let $_rpl_change_topology_privilege_checks_all=
--let $_rpl_change_topology_privilege_checks_all_index=

--let $include_filename= rpl/change_topology.inc
--source include/end_include_file.inc
