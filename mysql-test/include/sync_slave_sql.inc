# ==== Purpose ====
#
# This file does the same as the built-in command sync_with_master,
# but more flexible. In particular:
#  - Can use a custom timeout.
#  - Can use WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS instead of SOURCE_POS_WAIT.
#  - The position can be set by the user, it is not restricted to
#    save_master_pos
#
# ==== Usage ====
#
# --connection master
# --source include/save_master_pos.inc
# --connection slave
# [--let $slave_timeout= X]
# [--let $use_gtids= 1]
# [--let $wait_for_executed_gtid_set= 1]
# [--let $ignore_gtids_on_sync= 1]
# --source include/sync_slave.inc
#
# Parameters:
#   $slave_timeout
#     See wait_for_slave_param.inc
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.
#
#   $ignore_gtids_on_sync
#     Forces the use of master file and position, even if $use_gtids is set.
#     This might be used if the slave will not have all the GTIDs of the master
#     but have to read and apply all master events to the end.
#
#   $wait_for_executed_gtid_set
#     See include/sync_slave_sql_with_master.inc

--let $include_filename= sync_slave_sql.inc
--source include/begin_include_file.inc

--let $_sss_slave_timeout= $slave_timeout
if (!$_sss_slave_timeout)
{
  --let $_sss_slave_timeout= 300
}

if ($VALGRIND_TEST) {
  let $_sss_slave_timeout= `select $_sss_slave_timeout * 6`;
}

--let $_sss_channel_param=
if ($_saved_channel_name)
{
  --let $_sss_channel_param= , $_saved_channel_name
}

if ($rpl_debug)
{
  --echo use_gtids='$use_gtids' ignore_gtids_on_sync='$ignore_gtids_on_sync' _saved_gtids='$_saved_gtids' _saved_file='$_saved_file' _saved_pos='$_saved_pos' timeout='$_sss_slave_timeout'
}

#
# Verify if the SQL thread is already updated by the master position
# regardless the use if GTIDs. If the SQL thread is already updated,
# we can reduce the slave timeout of WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS
# to a minimum because there in nothing else to sync.
# As there is a possibility of missing GTIDs on slave, this reduced
# timeout will make mtr throw an error almost promptly, without having
# to wait 300 seconds to notice a sync problem between master and slave.
#
--let $_slave_master_file= query_get_value(SHOW SLAVE STATUS, Relay_Master_Log_File, 1)
--let $_slave_master_pos= query_get_value(SHOW SLAVE STATUS, Exec_Master_Log_Pos, 1)

if ($rpl_debug)
{
  --echo _saved_file='$_saved_file'
  --echo _slave_master_file='$_slave_master_file'
}

if ($_slave_master_file == $_saved_file)
{
  if ($rpl_debug)
  {
    --echo _saved_pos='$_saved_pos'
    --echo _slave_master_pos='$_slave_master_pos'
  }
  if ($_slave_master_pos == $_saved_pos)
  {
    if ($rpl_debug)
    {
      --echo debug: It seems that SQL thread is already synced (by verifying master file and position)
      --echo debug: Changing slave timeout to 1 seconds
    }
    --let $_sss_slave_timeout= 1
  }
}

--let $_sync_with_gtids= $use_gtids
if ($ignore_gtids_on_sync)
{
  --let $_sync_with_gtids= 0
}

if ($_sync_with_gtids)
{
  --source include/assert_gtid_mode_on.inc
  if ($wait_for_executed_gtid_set)
  {
    if ($rpl_debug)
    {
      --echo debug: WAIT_FOR_EXECUTED_GTID_SET('$_saved_gtids', $_sss_slave_timeout)
    }
    --let $_sss_result= `SELECT -WAIT_FOR_EXECUTED_GTID_SET('$_saved_gtids', $_sss_slave_timeout)`
  }
  if (!$wait_for_executed_gtid_set)
  {
    if ($rpl_debug)
    {
      --echo debug: WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('$_saved_gtids', $_sss_slave_timeout$_sss_channel_param)
    }
    --let $_sss_result= `SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('$_saved_gtids', $_sss_slave_timeout$_sss_channel_param)`
  }
}

if (!$_sync_with_gtids)
{
  if ($rpl_debug)
  {
    --echo debug: SOURCE_POS_WAIT('$_saved_file', $_saved_pos, $_sss_slave_timeout$_sss_channel_param)
  }
  --let $_sss_source_pos_wait_function = SOURCE_POS_WAIT
  if ($use_source_pos_wait_alias) {
    --let $_sss_source_pos_wait_function = MASTER_POS_WAIT
  }
  --let $_sss_result= `SELECT $_sss_source_pos_wait_function('$_saved_file', $_saved_pos, $_sss_slave_timeout$_sss_channel_param)`
}

if ($_sss_result == '')
{
  --let $_sss_result = -2
  --let $error_type= The slave SQL thread was not started, the slave's master information was not initialized, the arguments were incorrect, or an error occurred.
}
if ($_sss_result == -1)
{
  --let $error_type= Timeout after $_sss_slave_timeout seconds.
}

if ($_sss_result < 0)
{
  --source include/show_rpl_debug_info.inc
  --echo ERROR: sync_slave_sql.inc failed on connection '$CURRENT_CONNECTION'
  --echo ERROR: use_gtids='$use_gtids'
  --echo ERROR: _saved_gtids='$_saved_gtids'
  --echo ERROR: _saved_file='$_saved_file'
  --echo ERROR: _saved_pos='$_saved_pos'
  --echo ERROR: _saved_channel_name=$_saved_channel_name
  --echo ERROR: timeout='$_sss_slave_timeout'
  --echo ERROR: result='$_sss_result'
  --echo ERROR: error type: $error_type
  --die Error in sync_with_master.inc
}

--let $include_filename= sync_slave_sql.inc
--source include/end_include_file.inc
