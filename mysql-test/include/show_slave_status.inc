# ==== Purpose ====
#
# Show selected columns of output from SHOW SLAVE STATUS.
#
# Note: test cases should never call SHOW SLAVE STATUS directly,
# because that outputs more information to the query log than what is
# needed for the property that is being tested.  That would cause
# maintenance problems, because (1) it is hard for a human to
# understand what property is being tested; (2) the output of many of
# the fields is non-deterministic (e.g., Slave_IO_State) or changes
# frequently (e.g., binlog positions).
#
# Instead, what you want most of the time is to source one of the
# following scripts:
#
#   include/check_slave_no_error.inc
#     Assert that Slave_SQL_Errno = Slave_IO_Errno = 0.
#
#   include/check_slave_is_running.inc
#     Assert that Slave_IO_Running = Slave_SQL_Running = 'Yes'.
#
#   include/wait_for_slave_sql_error.inc
#     Wait for the SQL thread to get a given error (and assert that
#     it has this error).
#
#   include/wait_for_slave_io_error.inc
#     Wait for the IO thread to get a given error (and assert that
#     it has this error).
#
#   include/wait_for_slave_sql_to_stop.inc
#   include/wait_for_slave_io_to_stop.inc
#   include/wait_for_slave_to_stop.inc
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
# --source include/show_slave_status.inc
#
# Parameters:
#   $status_items
#     Set to the name of the column in the output of SHOW SLAVE STATUS
#     that you want to display. Example:
#
#       --let $status_items= Master_SSL_Allowed
#
#     You can show multiple columns by setting $status_items to a
#     comma-separated list. Example:
#
#       --let $status_items= Master_Log_File, Relay_Master_Log_File


--let $_show_slave_status_items=$status_items
if (`SELECT "XX$status_items" = "XX"`)
{
  --die Bug in test case: The mysqltest variable $status_items is not set.
}


while (`SELECT "XX$_show_slave_status_items" <> "XX"`) 
{
  --let $_show_slave_status_name= `SELECT SUBSTRING_INDEX('$_show_slave_status_items', ',', 1)`
  --let $_show_slave_status_items= `SELECT LTRIM(SUBSTRING('$_show_slave_status_items', LENGTH('$_show_slave_status_name') + 2))`

  --let $_show_slave_status_value= query_get_value(SHOW SLAVE STATUS, $_show_slave_status_name, 1)
  --let $_show_slave_status_value= `SELECT REPLACE("$_show_slave_status_value", '$MYSQL_TEST_DIR', 'MYSQL_TEST_DIR')`
  --echo $_show_slave_status_name = '$_show_slave_status_value'
}
