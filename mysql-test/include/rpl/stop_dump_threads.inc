# ==== Purpose ====
#
# Stop all dump threads on the server of the current connection.
#
# ==== Usage ====
#
# --source include/rpl/stop_dump_threads.inc

--let $include_filename= rpl/stop_dump_threads.inc
--source include/begin_include_file.inc


--let $_sdt_show_rpl_debug_info_old= $show_rpl_debug_info
--let $show_rpl_debug_info= 1
--disable_query_log
--disable_result_log

--let $_sdt_dump_thread_id= `SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE COMMAND = 'Binlog dump' OR COMMAND = 'Binlog Dump GTID'`

while ($_sdt_dump_thread_id != '')
{
  # error "ER_NO_SUCH_THREAD" may occur in case the Binlog Dump thread terminates
  # between the query "SELECT ID ..." and the KILL statement
  --error 0,ER_NO_SUCH_THREAD
  eval KILL $_sdt_dump_thread_id;
  --let $wait_condition= SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.PROCESSLIST WHERE ID = $_sdt_dump_thread_id
  --source include/wait_condition.inc

  --let $_sdt_dump_thread_id= `SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE COMMAND = 'Binlog dump' OR COMMAND = 'Binlog DUMP GTID'`
}


--let $show_rpl_debug_info= $_sdt_show_rpl_debug_info_old

--let $include_filename= rpl/stop_dump_threads.inc
--source include/end_include_file.inc
