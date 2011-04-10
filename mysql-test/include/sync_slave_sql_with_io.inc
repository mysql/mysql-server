# ==== Purpose ====
#
# Sync the slave SQL thread with the IO thread.
#
# ==== Usage ====
#
# [--let $slave_timeout= NUMBER]
# [--let $rpl_debug= 1]
# --source include/sync_slave_sql_with_io.inc
#
# Parameters:
#   $slave_timeout
#     By default, the synchronization timeouts after 300 seconds. If
#     $slave_timeout is set, the synchronization timeouts after
#     $slave_timeout seconds.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $include_filename= sync_slave_sql_with_io.inc
--source include/begin_include_file.inc


let $_slave_timeout= $slave_timeout;
if (!$_slave_timeout)
{
  let $_slave_timeout= 300;
}

--let $_master_log_file= query_get_value(SHOW SLAVE STATUS, Master_Log_File, 1)
--let $_master_log_pos= query_get_value(SHOW SLAVE STATUS, Read_Master_Log_Pos, 1)

if ($rpl_debug)
{
  --echo Master_Log_File='$_master_log_file' Read_Master_Log_Pos='$_master_log_pos' \$slave_timeout='$_slave_timeout'
}

--let $_sync_slave_sql_with_io_errno= `SELECT MASTER_POS_WAIT('$_master_log_file', $_master_log_pos, $_slave_timeout)`
if (`SELECT IFNULL($_sync_slave_sql_with_io_errno, -1) < 0`)
{
  --echo #### Failed to sync slave SQL thread with slave IO thread. ####
  --echo MASTER_POS_WAIT('$_master_log_file', $_master_log_pos, $_slave_timeout) returned $_sync_slave_sql_with_io_errno
  --source include/show_rpl_debug_info.inc
  --die Failed to sync slave SQL thread with slave IO thread.
}


--let $include_filename= sync_slave_sql_with_io.inc
--source include/end_include_file.inc
