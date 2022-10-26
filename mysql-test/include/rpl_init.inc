# ==== Purpose ====
#
# Set up replication on several servers in a specified topology.
#
# By default, this script does the following:
#  - Creates the connections server_1, server_2, ..., server_N, as
#    well as extra connections server_1_1, server_2_1, ...,
#    server_N_1. server_I and server_I_1 are connections to the same
#    server.
#  - Verifies that @@server_id of all servers are different.
#  - Calls RESET MASTER, RESET REPLICA, USE test, CHANGE REPLICATION SOURCE, START REPLICA.
#  - Sets the connection to server_1 before exiting.
#  - Sets the following mtr variables for convenience:
#    $gtid_mode: the value of @@global.gtid_mode on server 1
#    $gtid_mode_on: 1 if gtid_mode=on, 0 if gtid_mode=off
#    $binlog_format: The value of @@global.binlog_format.
#    $server_1_datadir, ..., $server_10_datadir: the @@datadir on each server.
#    $server_1_uuid, ..., $server_10_uuid: the @@server_uuid on each server.
#
# ==== Usage ====
#
# 1. If you are going to use more than two servers, create
#    rpl_test.cfg with the following contents:
#
#    !include ../my.cnf
#    [mysqld.1]
#    log-replica-updates
#    [mysqld.2]
#    log-replica-updates
#    ...
#    [mysqld.N]
#    log-replica-updates
#
#    [ENV]
#    SERVER_MYPORT_3= @mysqld.3.port
#    SERVER_MYPORT_4= @mysqld.4.port
#    SERVER_MYPORT_5= @mysqld.5.port
#    ...
#    SERVER_MYPORT_N= @mysqld.N.port
#
#    (It is allowed, but not required, to configure SERVER_MYPORT_1
#    and SERVER_MYPORT_2 too. If these variables are not set, the
#    variables MASTER_MYPORT and SLAVE_MYPORT, configured in the
#    default my.cnf used by the rpl and rpl_ndb suites, are used
#    instead.  In addition, in the rpl_ndb suite, SERVER_MYPORT_3 is
#    not needed since MASTER_MYPORT1 can be used instead.)
#
# 2. Execute the following near the top of the test:
#
#    [--let $rpl_server_count= 7]
#    --let $rpl_topology= 1->2->3->1->4, 2->5, 6->7
#    [--let $rpl_multi_source= 1]
#    [--let $rpl_extra_connections_per_server= 1]
#    [--let $rpl_check_server_ids= 1]
#    [--let $rpl_skip_change_master= 1]
#    [--let $rpl_skip_start_slave= 1]
#    [--let $rpl_skip_reset_master_and_slave= 1]
#    [--let $rpl_debug= 1]
#    [--let $slave_timeout= NUMBER]
#    [--let $use_gtids= 1]
#    [--let $rpl_group_replication= 1]
#    [--let $rpl_gtid_utils= 1]
#    [--let $rpl_set_gtid_next_anonymous_in_new_connection= 1]
#    [--let $rpl_skip_log_bin= 1]
#    [--let $rpl_privilege_checks_user= 1:'user'@'host',2:NULL]
#    [--let $rpl_privilege_checks_user_additional_grants= CREATE,INSERT,SELECT]
#    [--let $rpl_require_row_format= *:0,1:0,2:1]
#    [--let $rpl_require_table_primary_key_check= *:STREAM,1:ON,2:OFF,3:GENERATE]
#    [--let $rpl_gtid_only= *:0,1:0,2:1]
#    [--let $rpl_shallow_reset_slave= 1]
#    --source include/rpl_init.inc
#
#    Parameters:
#
#    $rpl_server_count
#      The number of servers to configure.  If this is not set, the largest
#      number in $rpl_topology will be used.
#
#    $rpl_topology
#      A comma-separated list of replication chain
#      specifications. Each replication chain specification has the
#      form S1->S2->...->Sn, where 1 <= S1,...Sn <= $rpl_server_count.
#      This file will configure S(i+1) to be a slave of S(i).  If you
#      want to specify the empty topology (no server replicates at
#      all), you have to set $rpl_topology=none.
#
#    $rpl_multi_source
#      By default, multi-source topologies are not allowed, and the
#      default channel is used always.  To enable multi-source, set
#      $rpl_multi_source=1.  Then, the empty channel is never used,
#      and the channel for connecting to server N is named
#      'channel_N'.  However, rpl_sync.inc is not usable in this case,
#      and rpl_end.inc must be used with the parameter.
#
#    $rpl_extra_connections_per_server
#      By default, this script creates connections server_N and
#      server_N_1.  If you can set this variable to a number, the
#      script creates:
#      server_N, server_N_1, ..., server_N_$rpl_extra_connections_per_server
#
#    $rpl_check_server_ids
#      If $rpl_check_server_ids is set, this script checks that the
#      @@server_id of all servers are different. This is normally
#      guaranteed by mtr, so it is only useful for debugging.
#
#    $rpl_skip_reset_master_and_slave
#      By default, this script issues RESET MASTER and RESET REPLICA
#      before CHANGE REPLICATION SOURCE and START REPLICA.  RESET MASTER
#      and RESET REPLICA are suppressed if $rpl_skip_reset_master_and_slave
#      is set.
#
#    $rpl_skip_change_master
#      By default, this script issues CHANGE REPLICATION SOURCE so that
#      all slaves are ready to run as specified by $rpl_topology. CHANGE
#      REPLICATION SOURCE is suppressed if $rpl_skip_change_master is set.
#
#    $rpl_skip_start_slave
#      By default, this script issues START REPLICA on all slaves
#      specified by $rpl_topology. START REPLICA is suppressed if
#      $rpl_skip_change_master is set.
#
#    $rpl_debug
#      By default, this script only outputs a static text that says
#      that rpl_init.inc was invoked. If $rpl_debug is set, additional
#      debug info is printed. The debug info may be nondeterministic,
#      so no test case should be checked in with $rpl_debug set.
#
#    $slave_timeout
#      Timeout used when waiting for the slave threads to start.
#      See include/wait_for_slave_param.inc
#
#    $use_gtids
#      Use option SOURCE_AUTO_POSITION = 1 to CHANGE REPLICATION SOURCE, and use
#      GTIDs in sync_*.inc. If the variable is not set, then this
#      script sets it to 1 if GTID_MODE = ON and 0 otherwise.
#
#    $rpl_group_replication
#      This checks for whether we have Group Replication enabled or not. Based
#      on the check this parameter we execute CHANGE REPLICATION SOURCE or not
#      in this inc file. Moreover, it also sets the default timeouts for the
#      group replication tests.
#
#    $rpl_gtid_utils
#      Create stored functions for GTID manipulation on every server (see
#      gtid_utils.inc).
#
#    $rpl_set_gtid_next_anonymous_in_new_connection
#       Executes SET GTID_NEXT = 'ANONYMOUS' in each new connection.
#
#    $rpl_skip_log_bin
#      Bypasses the binlog related checking to allow testing --skip-log-bin
#      servers.
#
#   $rpl_privilege_checks_user
#      See `include/rpl_change_topology.inc`.
#
#   $rpl_privilege_checks_user_additional_grants
#      See `include/rpl_change_topology.inc`.
#
#   $rpl_require_row_format
#      See `include/rpl_change_topology.inc`.
#
#   $rpl_require_table_primary_key_check
#      See `include/rpl_change_topology.inc`.
#
#   $rpl_gtid_only
#      See `include/rpl_change_topology.inc`.
#
#   $rpl_shallow_reset_slave
#      If in the setup we shall only use RESET REPLICA and not RESET REPLICA ALL
#
# ==== Side effects ====
#
# Changes current connection to server_1.
#
#
# ==== Internal variables configured by this file ====
#
# $rpl_inited
#   Set to 1 to indicate that this file has been sourced. This will
#   be set to '' again by rpl_end.inc.
#
# $_rpl_server_count
#   A backup of $rpl_server_count is stored in this variable. This is
#   used to perform a sanity check in rpl_change_topology.inc, to
#   check that $rpl_server_count does not between rpl_init.inc and
#   rpl_end.inc
#
# $rpl_server_count_length
#   Set to LENGTH($rpl_server_count). So if $rpl_server_count < 10,
#   then $rpl_server_count_length = 1; if 10 <= $rpl_server_count <
#   100, then $rpl_server_count_length = 2, etc.
#
# $_rpl_space
#   Contains a single space. This is used by other .inc files to
#   append a single space character to the end of an
#   $mysqltest_variable. This can't be done in any more direct way
#   since mysqltest trims whitespace from string literals.
#
# $_rpl_no_server
#   A sequence of $rpl_server_count_length spaces. Used internally by
#   rpl_generate_sync_chain.inc
#
# $rpl_connection_count
#   The number of master-slave connections. For instance, if
#   $rpl_topology is '1->3', this is equal to 1; if $rpl_topology is
#   '1->2->3->1', this is 3.
#
#

if (!$rpl_skip_log_bin)
{
  # The default behaviour is to run the binlog checking
  --source include/have_log_bin.inc
}

--let $include_filename= rpl_init.inc [topology=$rpl_topology]
--source include/begin_include_file.inc


if ($rpl_debug)
{
  --echo ---- Check input ----
  --echo MASTER_MYPORT='$MASTER_MYPORT' SLAVE_MYPORT='$SLAVE_MYPORT' MASTER_MYPORT1='$MASTER_MYPORT1' SLAVE_MYPORT1='$SLAVE_MYPORT1'
}

# Allow $MASTER_MYPORT as alias for $SERVER_MYPORT_1
if (!$SERVER_MYPORT_1)
{
  --let SERVER_MYPORT_1= $MASTER_MYPORT
}
# Allow $SLAVE_MYPORT as alias for $SERVER_MYPORT_2
if (!$SERVER_MYPORT_2)
{
  --let SERVER_MYPORT_2= $SLAVE_MYPORT
}
# Allow $MASTER_MYPORT1 as alias for $SERVER_MYPORT_3
# (this alias is used by rpl_ndb tests)
if (!$SERVER_MYPORT_3)
{
  --let SERVER_MYPORT_3= $MASTER_MYPORT1
}
# Allow $SLAVE_MYPORT1 as alias for $SERVER_MYPORT_4
# (this alias is used by rpl_ndb tests)
if (!$SERVER_MYPORT_4)
{
  --let SERVER_MYPORT_4= $SLAVE_MYPORT1
}
# Check that $rpl_server_count is set
if (!$rpl_server_count)
{
  if ($rpl_topology == none)
  {
    --let $rpl_server_count= 1
  }
  if ($rpl_topology != none)
  {
    --let $rpl_server_count= `SELECT REPLACE('$rpl_topology', '->', ',')`
    if (`SELECT LOCATE(',', '$rpl_server_count')`)
    {
      --let $rpl_server_count= `SELECT GREATEST($rpl_server_count)`
    }
  }
}
# Set $rpl_server_count_length and some auxiliary variables.
--let $rpl_server_count_length= `SELECT LENGTH('$rpl_server_count')`
--let $_rpl_server_count= $rpl_server_count
--let $_rpl_no_server= `SELECT REPEAT(' ', $rpl_server_count_length)`
--let $_rpl_space= `SELECT ' '`


--let $_rpl_extra_connections_per_server= $rpl_extra_connections_per_server
if ($_rpl_extra_connections_per_server == '')
{
  --let $_rpl_extra_connections_per_server= 1
}


if ($rpl_debug)
{
  --echo ---- Setup connections and reset each server ----
}


if (!$rpl_debug)
{
  --disable_query_log
}


--let $gtid_mode= `SELECT @@GLOBAL.GTID_MODE`
--let $gtid_mode_on= 0
if ($gtid_mode == 'ON')
{
  --let $gtid_mode_on= 1
}

--let $binlog_format= `SELECT @@GLOBAL.BINLOG_FORMAT`

# Create two connections to each server; reset master/slave, select
# database, set autoinc variables.
--let $_rpl_server= $rpl_server_count
--let $underscore= _

if ($group_replication_group_name == '')
{
  --let $group_replication_group_name= `SELECT UUID()`
}

# if in group replication mode, set the default timeouts
if($rpl_group_replication)
{
  # Reset default timeouts for the group replication tests
  --source include/group_replication_timeouts.inc
}

while ($_rpl_server)
{
  # Connect.
  --let $rpl_server_number= $_rpl_server
  --let $rpl_connection_name= server_$_rpl_server
  --source include/rpl_connect.inc
  --let $_rpl_connection_number= 1
  while ($_rpl_connection_number <= $_rpl_extra_connections_per_server)
  {
    --let $rpl_connection_name= server_$_rpl_server$underscore$_rpl_connection_number
    --source include/rpl_connect.inc
    --inc $_rpl_connection_number
  }

  # Configure server.
  --let $rpl_connection_name= server_$_rpl_server
  --source include/rpl_connection.inc
  USE test;
  if ($rpl_gtid_utils)
  {
    --source include/gtid_utils.inc
  }
  if (!$rpl_skip_reset_master_and_slave)
  {

    RESET MASTER;
    if (!$rpl_shallow_reset_slave){
      --let $_rpl_init_reset_slave_modifier= ALL
    }
    --eval RESET REPLICA $_rpl_init_reset_slave_modifier FOR CHANNEL ''
  }
  # Only way to do this in mtr :-(
  if ($_rpl_server == 1) {
    --let $server_1_datadir= `SELECT @@datadir`
    --let $server_1_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 2) {
    --let $server_2_datadir= `SELECT @@datadir`
    --let $server_2_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 3) {
    --let $server_3_datadir= `SELECT @@datadir`
    --let $server_3_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 4) {
    --let $server_4_datadir= `SELECT @@datadir`
    --let $server_4_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 5) {
    --let $server_5_datadir= `SELECT @@datadir`
    --let $server_5_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 6) {
    --let $server_6_datadir= `SELECT @@datadir`
    --let $server_6_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 7) {
    --let $server_7_datadir= `SELECT @@datadir`
    --let $server_7_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 8) {
    --let $server_8_datadir= `SELECT @@datadir`
    --let $server_8_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 9) {
    --let $server_9_datadir= `SELECT @@datadir`
    --let $server_9_uuid= `SELECT @@server_uuid`
  }
  if ($_rpl_server == 10) {
    --let $server_10_datadir= `SELECT @@datadir`
    --let $server_10_uuid= `SELECT @@server_uuid`
  }

  if($rpl_group_replication)
  {
    --source include/store_group_replication_auto_increment.inc
    if(!$rpl_skip_group_replication_start)
    {
      --source include/start_group_replication.inc
    }

    # Group Replication has its own auto_increment default value:
    #   GROUP_REPLICATION_AUTO_INCREMENT_INCREMENT= 7
    # which on Group Replication start will set server variables:
    #   auto_increment_increment= 7
    #   auto_increment_offset= SERVER_ID
    # This configuration enables conflict free auto-increment transactions
    # on a group. Please see WL#8445 for further details.
    #
    # Group Replication plugin is tested with two MTR suites:
    #  group_replication: tests written with GR in mind that exploits all
    #                     GR features.
    #  rpl:               tests that are written for asynchronous
    #                     replication, on which changes are written only on
    #                     master and synced asynchronously later on slaves.
    #
    # Tests from rpl suite are run for regression testing. Since these
    # assume that only master get changes, there is no need to use GR
    # auto_increment default value. Actually if we use it, that will make
    # the tests to fail with result content mismatch. Thence for rpl suite
    # we revert GR auto_increment default value.
    #
    # We can distinguish if we are running rpl or group_replication suite
    # by checking if rpl_group_replication_default_connections.inc file
    # was included. Only group_replication suite includes it.
    # So if the flag !$rpl_group_replication_default_connections is false,
    # we are currently running rpl suite, which means that we need to
    # revert GR auto_increment default value.

    if (!$using_rpl_group_replication_default_connections)
    {
      --source include/restore_group_replication_auto_increment.inc
    }
  }
  --dec $_rpl_server
}


# Set rpl_inited to indicate that initialization is done.
if ($rpl_inited)
{
  --die Detected double invocation of rpl_init.inc. Use rpl_change_topology.inc or invoke rpl_end.inc before the second invocation of rpl_init.inc
}
--let $rpl_inited= 1

# Set $show_rpl_debug_info to make subsequent failures in
# e.g. wait_condition.inc print replication debug output.
--let $show_rpl_debug_info= 1


# Indicate that the server is in a dirty state and needs to be restarted
# if the test is skipped. If the test is not skipped, it will continue
# to the end and execute its cleanup section (and check-testcase will
# report if you forget to clean up).
--source include/force_restart_if_skipped.inc


# Assert that all hosts have different server_ids
if ($rpl_check_server_ids)
{
  if ($rpl_debug)
  {
    --echo ---- Check that @@server_id is distinct for all servers ----
  }

  --let $_rpl_server= $rpl_server_count
  while ($_rpl_server)
  {
    --let $_rpl_server2= $_rpl_server
    --dec $_rpl_server2
    while ($_rpl_server2)
    {
      --let $assert_text= Servers $_rpl_server and $_rpl_server2 should have different @@server_id
      --let $assert_condition= [$_rpl_server:SELECT @@server_id AS i, i, 1] != [$_rpl_server2:SELECT @@server_id AS i, i, 1]

      --source include/assert.inc
      --dec $_rpl_server2
    }
    --dec $_rpl_server
  }
}

# Determine if we should use gtids or not.
if ($use_gtids == '')
{
  --let $use_gtids= $gtid_mode_on
}


--source include/rpl_change_topology.inc

# reconnect group_replication default connection
# so session variables values can be
# reinitialized with its corresponding global value.
if($rpl_group_replication)
{
  if ($using_rpl_group_replication_default_connections)
  {
    --let $rpl_group_replication_reconnect= 1
    --source include/rpl_group_replication_default_connections.inc
  }
}

if (!$rpl_skip_start_slave)
{
  if(!$rpl_group_replication)
  {
    --source include/rpl_start_slaves.inc
  }
}


--let $rpl_connection_name= server_1
--source include/rpl_connection.inc


--let $skip_restore_connection= 1
--let $include_filename= rpl_init.inc
--source include/end_include_file.inc
