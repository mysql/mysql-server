# ==== Purpose ====
#
# This file does the same as the built-in command sync_with_master,
# but more flexible. In particular:
#  - Can use a custom timeout.
#  - Can use WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS instead of MASTER_POS_WAIT.
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
# --source include/sync_slave.inc
#
# Parameters:
#   $slave_timeout
#     See wait_for_slave_param.inc
#
#   $use_gtids
#     If set, uses GTIDs instead of filename and offset for positions.

--let $include_filename= sync_slave_sql.inc
--source include/begin_include_file.inc

--let $_sss_slave_timeout= $slave_timeout
if (!$_sss_slave_timeout)
{
  --let $_sss_slave_timeout= 300
}

if ($rpl_debug)
{
  --echo use_gtids='$use_gtids' _saved_gtids='$_saved_gtids' _saved_file='$_saved_file' _saved_pos='$_saved_pos' timeout='$_sss_slave_timeout'
}

if ($use_gtids)
{
  if ($rpl_debug)
  {
    --echo debug: WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('$_saved_gtids', $_sss_slave_timeout)
  }
  --let $_sss_result= `SELECT WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS('$_saved_gtids', $_sss_slave_timeout)`
}
if (!$use_gtids)
{
  if ($rpl_debug)
  {
    --echo debug: MASTER_POS_WAIT('$_saved_file', $_saved_pos, $_sss_slave_timeout)
  }
  --let $_sss_result= `SELECT MASTER_POS_WAIT('$_saved_file', $_saved_pos, $_sss_slave_timeout)`
}

if ($_sss_result == '')
{
  --let $_sss_result = -2
  --let $error_type= The slave SQL thread was not started, the slave's master information was not initialized, the arguments were incorrect, or an error occurred.
}
if ($_sss_result == -1)
{
  --let $error_type= Timeout after $slave_timeout seconds.
}

if ($_sss_result < 0)
{
  --source include/show_rpl_debug_info.inc
  --echo ERROR: sync_slave_sql.inc failed on connection '$CURRENT_CONNECTION'
  --echo ERROR: use_gtids='$use_gtids'
  --echo ERROR: _saved_gtids='$_saved_gtids'
  --echo ERROR: _saved_file='$_saved_file'
  --echo ERROR: _saved_pos='$_saved_pos'
  --echo ERROR: timeout='$_sss_slave_timeout'
  --echo ERROR: result='$_sss_result'
  --echo ERROR: error type: $error_type
  --die Error in sync_with_master.inc
}

--let $include_filename= sync_slave_sql.inc
--source include/end_include_file.inc
