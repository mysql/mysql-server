# ==== Purpose ====
#
# Show selected columns of output from SHOW REPLICA STATUS FOR CHANNEL channel.
#
# Note: test cases should never call SHOW REPLICA STATUS FOR CHANNEL channel,
# directly, because that outputs more information to the query log than
# what is needed for the property that is being tested.  That would
# cause maintenance problems, because (1) it is hard for a human to
# understand what property is being tested; (2) the output of many of
# the fields is non-deterministic (e.g., Replica_IO_State) or changes
# frequently (e.g., binlog positions).
#
# Instead, what you want most of the time is to source one of the
# following scripts:
#
#   include/rpl/assert_replica_no_error.inc
#     Assert that Slave_SQL_Errno = Slave_IO_Errno = 0.
#
#   include/rpl/assert_replica_running.inc
#     Assert that Replica_IO_Running = Replica_SQL_Running = 'Yes'.
#
#   include/rpl/wait_for_applier_error.inc
#     Wait for the SQL thread to get a given error (and assert that
#     it has this error).
#
#   include/rpl/wait_for_receiver_error.inc
#     Wait for the IO thread to get a given error (and assert that
#     it has this error).
#
#   include/rpl/wait_for_applier_to_stop.inc
#   include/rpl/wait_for_receiver_to_stop.inc
#   include/rpl/wait_for_replica_to_stop.inc
#     Wait for the SQL thread, the IO thread, or both to stop (and
#     assert they stop).
#
# When none of the above applies, you may use this script instead.
# However, take care so that the test never contains explicit binlog
# coordinates. Usually you can read the binlog coordinates into a
# variable and compare it to some other coordinates.
#
#
# ==== Usage ====
#
# --let $status_items= Column_Name[, Column_Name[, ...]]
# [--let $rpl_channel_name=channel_name]
# --source include/rpl/deprecated/show_replica_status.inc
#
# Parameters:
#   $status_items
#     Set to the name of the column in the output of SHOW REPLICA STATUS
#     that you want to display. Example:
#
#       --let $status_items= Source_SSL_Allowed
#
#     You can show multiple columns by setting $status_items to a
#     comma-separated list. Example:
#
#       --let $status_items= Source_Log_File, Relay_Source_Log_File
#
#   $slave_field_result_replace
#     If set, one or more regex patterns for replacing variable
#     text in the error message. Syntax as --replace-regex
#
#   $slave_sql_mode
#     If set, change the slave sql mode during this macro, reverting
#     to the previous on exit. Default sql_mode is NO_BACKSLASH_ESCAPES
#     to allow replace '\' by '/' making paths OS independent. Example:
#
#       --let $slave_sql_mode= NO_BACKSLASH_ESCAPES
#
#   $rpl_channel_name
#     If set, FOR CHANNEL clause is added to SHOW REPLICA STATUS command.
#

--let $_show_slave_status_items= $status_items
if (!$status_items)
{
  --die Bug in test case: The mysqltest variable $status_items is not set.
}

--let $_show_slave_status_channel= $rpl_channel_name
if (!$rpl_channel_name)
{
  --let $_show_slave_status_channel= ''
}

--let $_slave_sql_mode= NO_BACKSLASH_ESCAPES
if ($slave_sql_mode)
{
  --let $_slave_sql_mode= $slave_sql_mode
}
--let $_previous_slave_sql_mode = `SELECT @@sql_mode`
--disable_query_log
eval SET sql_mode= '$_slave_sql_mode';
--enable_query_log

--let $show_slave_status_cmd=SHOW REPLICA STATUS
if ($rpl_channel_name)
{
  --let $show_slave_status_cmd= $show_slave_status_cmd FOR CHANNEL $rpl_channel_name
}

while ($_show_slave_status_items)
{
  --let $_show_slave_status_name= `SELECT SUBSTRING_INDEX('$_show_slave_status_items', ',', 1)`
  --let $_show_slave_status_items= `SELECT LTRIM(SUBSTRING('$_show_slave_status_items', LENGTH('$_show_slave_status_name') + 2))`

  --let $_show_slave_status_value= query_get_value($show_slave_status_cmd, $_show_slave_status_name, 1)

  --let $_show_slave_status_value = escape("\,$_show_slave_status_value)
  --let $_slave_field_result_replace= /[\\\\]/\// $slave_field_result_replace
  --replace_regex $_slave_field_result_replace
  --let $_show_slave_status_value= `SELECT REPLACE("$_show_slave_status_value", '$MYSQL_TEST_DIR', 'MYSQL_TEST_DIR')`
  --echo $_show_slave_status_name = '$_show_slave_status_value'
}


--disable_query_log
eval SET sql_mode= '$_previous_slave_sql_mode';
--enable_query_log
