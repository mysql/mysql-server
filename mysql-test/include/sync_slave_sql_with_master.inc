# ==== Purpose ====
#
# Waits until the slave SQL thread has been synced, i.e., all events
# have been copied over to slave.  This is like mtr's built-in command
# sync_slave_with_master, but more flexible (e.g., you can set a
# custom timeout and you can force it to use GTIDs instead of filename
# and offset).
#
#
# ==== Usage ====
#
# [--let $sync_slave_connection= <connection_name>]
# [--let $use_gtids= 1]
# [--let $rpl_channel_name= NAME]
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# [--let $wait_for_executed_gtid_set= 1]
# [--let $ignore_gtids_on_sync= 1]
# --source include/sync_slave_sql_with_master.inc
#
# Must be called on the master. Will change connection to the slave.
#
# Parameters:
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $sync_slave_connection
#     By default, this script switches connection to 'slave'. If
#     $sync_slave_connection is set, then '$sync_slave_connection' is
#     used instead of 'slave'.
#
#   $rpl_channel_name
#     See include/wait_for_slave_param.inc
#
#   $slave_timeout
#      See include/wait_for_slave_param.inc.
#
#   $rpl_debug
#      See include/rpl_init.inc
#
#   $ignore_gtids_on_sync
#     Forces the use of master file and position, even if $use_gtids is set.
#     This might be used if the slave will not have all the GTIDs of the
#     master but have to read and apply all master events to the end.
#
#   $wait_for_executed_gtid_set
#      This option makes the test use WAIT_FOR_EXECUTED_GTID_SET instead of
#      WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS. This options also requires
#      $use_gtids and !$ignore_gtids_on_sync to be set.
#

--let $include_filename= sync_slave_sql_with_master.inc
--source include/begin_include_file.inc

--source include/wait_for_ndb_committed_to_binlog.inc
--source include/save_master_pos.inc

--let $rpl_connection_name= slave
if ($sync_slave_connection)
{
  --let $rpl_connection_name= $sync_slave_connection
}

# Assert that source and destination connections are not the same.
if ($rpl_connection_name == $CURRENT_CONNECTION)
{
  --echo Trying to sync '$CURRENT_CONNECTION' with '$rpl_connection_name' which are the same server!
  --echo Please double check your test case.
  --die
}
--source include/rpl_connection.inc

--source include/sync_slave_sql.inc

--let $include_filename= sync_slave_sql_with_master.inc
--let $skip_restore_connection= 1
--let $_saved_channel_name=
--source include/end_include_file.inc
