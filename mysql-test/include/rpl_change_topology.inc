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
# [--let $rpl_skip_change_master= 1]
# [--let $rpl_master_log_file= 1:master-bin.000001,3:master-bin.000003]
# [--let $rpl_master_log_pos= 1:4711,3:107]
# [--let $rpl_debug= 1]
# --source include/rpl_change_topology.inc
#
# Parameters:
#   $rpl_master_log_file
#     By default, CHANGE MASTER is executed with MASTER_LOG_FILE set
#     to the name of the last binlog file on the master (retrieved by
#     executing SHOW MASTER STATUS). This variable can be set to
#     specify another filename.  This variable should be a
#     comma-separated list of the following form:
#
#       SERVER_NUMBER_1:FILE_NAME_1,SERVER_NUMBER_2:FILE_NAME_2,...
#
#     Before CHANGE MASTER is executed on server N, this script checks
#     if $rpl_master_log_file contains the text N:FILE_NAME. If it
#     does, then MASTER_LOG_FILE is set to FILE_NAME. Otherwise,
#     MASTER_LOG_FILE is set to the last binlog on the master.  For
#     example, to specify that server_1 should start replicate from
#     master-bin.000007 and server_5 should start replicate from
#     master-bin.012345, do:
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
#   $rpl_server_count, $rpl_topology, $rpl_debug, $rpl_skip_change_master
#     See include/rpl_init.inc
#
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
# $rpl_server_count_length:
#   Set to LENGTH($rpl_server_count). So if $rpl_server_count < 10,
#   then $rpl_server_count_length = 1; if 10 <= $rpl_server_count <
#   100, then $rpl_server_count_length = 2, etc.
#
# $rpl_master_list
#   Set to a string consisting of $rpl_server_count numbers, each one
#   whitespace-padded to $rpl_server_count_length characters. If
#   server N is a slave, then the N'th number is the master of server
#   N. If server N is not a slave, then the N'th number is just spaces
#   (so in fact it is not a number). For example, if $rpl_topology is
#   '1->2,2->3,3->1,2->4,5->6', then $rpl_master_list is '3122 6'.
#
# $rpl_sync_chain_dirty
#   This variable is set to 1.  This tells include/rpl_sync.inc to
#   compute a new value for $rpl_sync_chain next time that
#   include/rpl_sync.inc is sourced.  See
#   include/rpl_generate_sync_chain.inc and include/rpl_sync.inc for
#   details.


# Remove whitespace from $rpl_topology
--let $rpl_topology= `SELECT REPLACE('$rpl_topology', ' ', '')`

--let $include_filename= rpl_change_topology.inc [new topology=$rpl_topology]
--source include/begin_include_file.inc


if ($rpl_debug)
{
  --echo ---- Check input ----
}


if (`SELECT '$rpl_topology' = '' OR '$rpl_server_count' = ''`)
{
  --die You must set $rpl_topology and $rpl_server_count before you source rpl_change_topology.inc. If you really want to change to the empty topology, set $rpl_topology= none
}
--let $_rpl_topology= $rpl_topology
if (`SELECT '$_rpl_topology' = 'none'`)
{
  --let $_rpl_topology=
}
if (`SELECT '!$rpl_master_list!' = '!!'`)
{
  --die You must source include/rpl_init.inc before you source include/rpl_change_topology.inc
}
--let $_rpl_old_master_list= $rpl_master_list

if ($rpl_debug)
{
  --echo \$rpl_server_count='$rpl_server_count'
  --echo \$rpl_server_count_length='$rpl_server_count_length'
  --echo new \$rpl_topology='$_rpl_topology'
  --echo old \$rpl_master_list='$rpl_master_list'
  --echo old \$rpl_sync_chain='$rpl_sync_chain'
}


if ($rpl_debug)
{
  --echo ---- Generate \$rpl_server_count_length and \$rpl_master_list ----
}

--let $rpl_server_count_length= `SELECT LENGTH('$rpl_server_count')`
--let $rpl_master_list=
--let $_rpl_no_server= `SELECT REPEAT(' ', $rpl_server_count_length)`
--let $rpl_master_list= `SELECT REPEAT('$_rpl_no_server', $rpl_server_count)`
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
  --let $_rpl_master= `SELECT SUBSTRING_INDEX('$_rpl_master_slave', '->', 1)`
  # Get 's2' from 's1->s2'
  --let $_rpl_slave= `SELECT SUBSTRING('$_rpl_master_slave', LENGTH('$_rpl_master') + 3)`
  # Check that s2 does not have another master.
  if (`SELECT SUBSTR('$rpl_master_list', 1 + ($_rpl_slave - 1) * $rpl_server_count_length, $rpl_server_count_length) != '$_rpl_no_server'`)
  {
    --echo ERROR IN TEST: Server '$_rpl_slave' has more than one master in topology '$rpl_topology'
    --die ERROR IN TEST: found a server with more than one master in the $rpl_topology variable
  }
  # Save 's1' at position 's2' in $rpl_master_list
  --let $rpl_master_list= `SELECT INSERT('$rpl_master_list', 1 + ($_rpl_slave - 1) * $rpl_server_count_length, $rpl_server_count_length, RPAD('$_rpl_master', $rpl_server_count_length, ' '))`
}

if ($rpl_debug)
{
  --echo new \$rpl_server_count_length = '$rpl_server_count_length'
  --echo new \$rpl_master_list = '$rpl_master_list'
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

  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    # The following statement evaluates to:
    #  0, if server_$_rpl_server has the same master as before.
    #  The master's server, if server_$_rpl_server is a slave.
    #  The empty string, if server_$_rpl_server is not a slave.
    --let $_rpl_master= `SELECT TRIM(IFNULL(NULLIF(SUBSTRING('$rpl_master_list', 1 + ($_rpl_server - 1) * $rpl_server_count_length, $rpl_server_count_length), SUBSTRING('$_rpl_old_master_list', 1 + ($_rpl_server - 1) * $rpl_server_count_length, $rpl_server_count_length)), 0))`
    if ($rpl_debug)
    {
      --echo \$_rpl_server='$_rpl_server' \$_rpl_master='$_rpl_master'
    }
    if ($_rpl_master)
    {
      # Get port number
      --let $_rpl_port= \$SERVER_MYPORT_$_rpl_master
      # Get MASTER_LOG_FILE
      --let $_rpl_master_log_file_index= `SELECT LOCATE('$_rpl_server:', '$rpl_master_log_file')`
      if ($_rpl_master_log_file_index)
      {
        # Get text from after ':' and before ',', starting at
        # $_rpl_master_log_file
        --let $_rpl_master_log_file= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_master_log_file', $_rpl_master_log_file_index), ',', 1), ':', -1)`
      }
      if (!$_rpl_master_log_file_index)
      {
        --let $rpl_connection_name= server_$_rpl_master
        --source include/rpl_connection.inc
        --let $_rpl_master_log_file= query_get_value(SHOW MASTER STATUS, File, 1)
      }
      # Change connection.
      --let $rpl_connection_name= server_$_rpl_server
      --source include/rpl_connection.inc
      # Get MASTER_LOG_POS
      --let $_rpl_master_log_pos_index= `SELECT LOCATE('$_rpl_server:', '$rpl_master_log_pos')`
      if ($_rpl_master_log_pos_index)
      {
        --let $_rpl_master_log_pos= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING('$rpl_master_log_pos', $_rpl_master_log_pos_index), ',', 1), ':', -1)`
        --let $_rpl_master_log_pos= , MASTER_LOG_POS = $_rpl_master_log_pos
      }
      if (!$_rpl_master_log_pos_index)
      {
        --let $_rpl_master_log_pos=
      }
      eval CHANGE MASTER TO MASTER_HOST = '127.0.0.1', MASTER_PORT = $_rpl_port, MASTER_USER = 'root', MASTER_LOG_FILE = '$_rpl_master_log_file'$_rpl_master_log_pos, MASTER_CONNECT_RETRY = 1;
    }
    if (!$_rpl_master)
    {
      if (`SELECT '$_rpl_master' = ''`)
      {
        --let $rpl_connection_name= server_$_rpl_server
        --source include/rpl_connection.inc
        CHANGE MASTER TO MASTER_HOST = '';
      }
    }
    --dec $_rpl_server
  }
}


--let $rpl_sync_chain_dirty= 1


--let $include_filename= rpl_change_topology.inc
--source include/end_include_file.inc
