# ==== Purpose ====
#
# Print the contents of all binary logs to the result log. This must
# only be used for debugging.
#
# ==== Usage ====
#
# [--let $output_file= FILENAME]
# [--let $append= 1]
# [--let $rpl_debug= 1]
# --source include/rpl/debug/show_all_binlogs.inc
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
#     See rpl/init.inc

--let $include_filename= rpl/debug/show_all_binlogs.inc
--source include/begin_include_file.inc


--disable_query_log

if ($output_file == '')
{
  --echo DO_NOT_CHECK_IN_THIS_LINE: include/rpl/debug/show_all_binlogs.inc should only be used for debugging. Never check in a test that calls it on success.
}

--let $_sab_number = 1
--let $_sab_binlog_file = query_get_value(SHOW BINARY LOGS, Log_name, $_sab_number)
while ($_sab_binlog_file != 'No such row')
{
  --let $_sab_statement = SHOW BINLOG EVENTS IN '$_sab_binlog_file'
  --let $_sab_header= ======== $_sab_statement ========

  if ($output_file != '')
  {
    --let $write_var = $_sab_header
    --let $write_to_file = $output_file
    --source include/write_var_to_file.inc

    --source include/write_result_to_file.inc
  }
  if ($output_file == '')
  {
    --echo $_sab_header
    eval $_sab_statement;
  }
  --inc $_sab_number

  --let $_sab_binlog_file = query_get_value(SHOW BINARY LOGS, Log_name, $_sab_number)
}


--let $include_filename = rpl/debug/show_all_binlogs.inc
--source include/end_include_file.inc
