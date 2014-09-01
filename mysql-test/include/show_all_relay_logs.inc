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

--let $last_relay_log_file= query_get_value(SHOW SLAVE STATUS, Relay_Log_File, 1)
--let $relay_log_base= `SELECT SUBSTRING_INDEX('$last_relay_log_file', '.', 1)`
--let $relay_log_count= `SELECT CONCAT('1', SUBSTRING_INDEX('$last_relay_log_file', '.', -1)) - 1000000`
--let $number= 1
while ($number <= $relay_log_count)
{
  --let $padded_number= `SELECT LPAD('$number', 6, '0')`
  --let $statement= SHOW RELAYLOG EVENTS IN '$relay_log_base.$padded_number'
  --let $header= ======== $statement ========

  if ($output_file != '')
  {
    --let $write_to_file= $output_file
    --let $write_var= $header
    --let $allow_error= 1
    --source include/write_var_to_file.inc

    --source include/write_result_to_file.inc
  }
  if ($output_file == '')
  {
    --echo $header
    --error 0, 1220 # 1220 = ER_ERROR_WHEN_EXECUTING_COMMAND
    eval $statement;
  }

  --inc $number
}


--source include/end_include_file.inc
