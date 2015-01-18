# ==== Purpose ====
#
# Changes replication topology. This file is normally sourced from
# include/rpl_init.inc, but test cases can also source it if they
# need to change topology after they have sourced include/rpl_init.inc
#
# This file sets up variables needed by include/rpl_sync.inc and many
# other replication scripts in the include/ directory.  It also issues
# CHANGE MASTER on all servers where the configuration changes from
# what it was before.  It does not issue START SLAVE (use
# include/rpl_start_slaves.inc for that).
#
# Note: it is not currently possible to change the number of servers
# after the rpl_init.inc, without first calling rpl_end.inc. So the
# test has to set $rpl_server_count to the total number of servers
# that the test uses, before it sources include/rpl_init.inc.  After
# that, $rpl_server_count must not change until after next time the
# test sources include/rpl_end.inc.
#
# Note: Since this script issues CHANGE MASTER, the test case must
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
# [--let $rpl_master_log_file= 1:master-bin.000001,3:master-bin.000003]
# [--let $rpl_master_log_pos= 1:4711,3:107]
# [--let $rpl_debug= 1]
# [--let $rpl_unconditional_change_master= 1]
# --source include/rpl_change_topology.inc
#
# Parameters:
#   $use_gtids
#     Use option MASTER_AUTO_POSITION = 1 to CHANGE MASTER.
#
#   $rpl_master_log_file
#     By default, CHANGE MASTER is executed without specifying the
#     MASTER_LOG_FILE parameter. This variable can be set to specify a
#     filename.  This variable should be a comma-separated list of the
#     following form:
#
#       SERVER_NUMBER_1:FILE_NAME_1,SERVER_NUMBER_2:FILE_NAME_2,...
#
#     Before CHANGE MASTER is executed on server N, this script checks
#     if $rpl_master_log_file contains the text N:FILE_NAME. If it
#     does, then MASTER_LOG_FILE is set to FILE_NAME. Otherwise,
#     MASTER_LOG_FILE is not specified.  For example, to specify that
#     server_1 should start replicate from master-bin.000007 and
#     server_5 should start replicate from master-bin.012345, do:
#
#       --let $rpl_master_log_file= 1:master-bin.000007,5:master-bin.012345
#
#   $rpl_master_log_pos
#     By default, CHANGE MASTER is executed without specifying the
#     MASTER_LOG_POS parameter.  This variable can be set to set a
#     specific position.  It has the same form as $rpl_master_log_file
#     (see above).  For example, to specify that server_3 should start
#     replicate from position 4711 of its master, do:
#       --let $rpl_master_log_pos= 3:4711
#
#   $rpl_unconditional_change_master
#     This script remembers the topology set by previous invokations
#     of either rpl_init.inc or of this script.  By default, this
#     script only executes CHANGE MASTER on servers that actually have
#     a new master.  If $rpl_unconditiona_change_master is set, then
#     this script unconditionally executes CHANGE MASTER on all
#     servers that have a master.  This is useful either if you have
#     changed the topology without invoking the scripts or if you want
#     to set some other parameters of CHANGE MASTER.
#
#   $rpl_server_count, $rpl_topology, $rpl_debug,
#   $rpl_skip_change_master, $rpl_multi_source
#     See include/rpl_init.inc
#
#   $rpl_group_replication
#     This checks for whether we have Group Replication enabled or not. Based
#     on the check this parameter we execute change master or not in this
#     inc file.
#
# ==== Internal variables configured by this file ====
#
# This file sets up the following variables, which are used by other
# low-level replication files such as:
#   include/rpl_sync.inc
#   include/rpl_start_slaves.inc
#   include/rpl_stop_slaves.inc
#   include/rpl_end.inc
#
# $rpl_connection_list
#   This variable will be set to a string consisting of N master-slave
#   specifications, where N is the number of master-slave
#   connections. Each master-slave specification has the form
#   'm<NUMBER1> s<NUMBER2> '. This indicates that server <NUMBER1> is a
#   direct master of server <NUMBER2>. Each <NUMBER> is padded to the
#   right by spaces to a fixed width of $rpl_server_count_length.  For
#   example, if $rpl_topology is '1->2->3->1,2->4,5->6', then
#   $rpl_connection_list is 'm1 s2 m2 s3 m3 s1 m2 s4 m5 s6'.
#   The length of this variable is exactly
#   2*(2+$rpl_connection_count_length)*$rpl_connection_list.
#
# $rpl_sync_chain_dirty
#   This variable is set to 1.  This tells include/rpl_sync.inc to
#   compute a new value for $rpl_sync_chain next time that
#   include/rpl_sync.inc is sourced.  See
#   include/rpl_generate_sync_chain.inc and include/rpl_sync.inc for
#   details.
#
# See also rpl_init.inc


# Remove whitespace from $rpl_topology
--let $rpl_topology= `SELECT REPLACE('$rpl_topology', ' ', '')`

--let $include_filename= rpl_change_topology.inc [new topology=$rpl_topology]
--source include/begin_include_file.inc


if ($rpl_debug)
{
  --echo ---- Check input ----
}

# $rpl_server_count must not change between rpl_init.inc and rpl_end.inc
if ($rpl_server_count != $_rpl_server_count)
{
  --die ERROR IN TEST: Detected that $rpl_server_count has changed. You are not allowed to change this between rpl_init.inc and rpl_end.inc.
}

# $rpl_topology must be set
if ($rpl_topology == '')
{
  --die ERROR IN TEST: You must set $rpl_topology before you source include/rpl_change_topology.inc. If you really want to change to the empty topology, set $rpl_topology= none
}
--let $_rpl_topology= $rpl_topology
if ($_rpl_topology == 'none')
{
  --let $_rpl_topology=
}

# include/rpl_init.inc should be invoked before this file
if (!$rpl_inited)
{
  --die ERROR IN TEST: You must source include/rpl_init.inc before you source include/rpl_change_topology.inc
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

if (!$rpl_skip_change_master)
{
  if ($rpl_debug)
  {
    --echo ---- Execute CHANGE MASTER on all servers ----
  }

  if (!$rpl_debug)
  {
    --disable_query_log
  }

  --let $rpl_source_file= include/rpl_change_topology_helper.inc
  --source include/rpl_for_each_connection.inc
}


--let $rpl_sync_chain_dirty= 1


--let $include_filename= rpl_change_topology.inc
--source include/end_include_file.inc
