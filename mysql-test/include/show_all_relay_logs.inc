# ==== Purpose ====
#
# Print the contents of all relay logs to the result log. This must
# only be used for debugging.
#
# ==== Usage ====
#
# [--let $output_file= FILENAME]
# [--let $append= 1]
# [--let $rpl_debug= 1]
# --source include/show_all_binlogs.inc
#
# Parameters:
#
#   $output_file
#     By default, output is printed to the result log.  If you need to
#     print it to a specific file, specify the filename with this
#     parameter.
#
#   $append
#     By default, if $output_file is specified and the file exists,
#     this script truncates the file before it writes to it.  If
#     $append is specified, the file is not truncated and the contents
#     of the binary log is appended to the end.
#
#   $rpl_debug
#     See rpl_init.inc

--let $include_filename= show_all_relay_logs.inc
--source include/begin_include_file.inc


--disable_query_log

if ($output_file == '')
{
  --echo DO_NOT_CHECK_IN_THIS_LINE: include/show_all_relay_logs.inc should only be used for debugging. Never check in a test that calls it on success.
}

--let $_sarl_table_name= `SELECT CONCAT('relay_logs_', REPLACE(UUID(), '-', '_'))`
--let $_sarl_sql_log_bin_old= `SELECT @@SESSION.sql_LOG_BIN`
SET SESSION SQL_LOG_BIN = 0;
eval CREATE TEMPORARY TABLE $_sarl_table_name
            (id INT PRIMARY KEY AUTO_INCREMENT, filename VARCHAR(512));

--let $_sarl_old_index_file= $index_file
--let $_sarl_old_table= $table
--let $index_file= `SELECT @@GLOBAL.RELAY_LOG_INDEX`
--let $table= $_sarl_table_name
--source include/rpl_read_binlog_index_into_table.inc
--let $index_file= $_sarl_old_index_file
--let $table= $_sarl_old_table

--let $_sarl_count= `SELECT COUNT(*) FROM $_sarl_table_name`

--echo ######## Showing $_sarl_count relay logs ########

--let $_sarl_number= 0
while ($_sarl_number < $_sarl_count)
{
  --let $_sarl_file= `SELECT filename FROM $_sarl_table_name LIMIT $_sarl_number, 1`
  --let $statement= SHOW RELAYLOG EVENTS IN '$_sarl_file'
  --let $_sarl_header= ======== $statement ========

  if ($output_file != '')
  {
    --let $write_to_file= $output_file
    --let $write_var= $_sarl_header
    --source include/write_var_to_file.inc

    --source include/write_result_to_file.inc
  }
  if ($output_file == '')
  {
    --echo $_sarl_header
    #    --error 0, 1220 # 1220 = ER_ERROR_WHEN_EXECUTING_COMMAND
    eval $statement;
  }

  --inc $_sarl_number
}

eval DROP TEMPORARY TABLE $_sarl_table_name;
eval SET SESSION SQL_LOG_BIN= $_sarl_sql_log_bin_old;

--let $include_filename= show_all_relay_logs.inc
--source include/end_include_file.inc
