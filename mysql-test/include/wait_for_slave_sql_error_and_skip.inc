# ==== Purpose ====
#
# Wait for slave SQL error, skip the erroneous statement and restart
# slave
#
# ==== Usage ====
#
# let $slave_sql_error= <ERRNO>;
# source include/wait_for_slave_sql_error_and_skip.inc;
#
# Parameters:
#
# $slave_sql_errno
#   The error number to wait for. This is required. (See
#   wait_for_slave_sql_error.inc)
#
# $show_sql_error
#   If set, will print the error to the query log.
#
# $slave_timeout
#   See wait_for_slave_param.inc for description.
# 
# $master_connection
#   See wait_for_slave_param.inc for description.
#
# $slave_skip_counter
#   If set, skip this number of events. If not set, skip one event.
#
# $not_switch_connection If set, don't switch to slave and don't switch back
# master.
#

echo --source include/wait_for_slave_sql_error_and_skip.inc;
if (!$not_switch_connection)
{
  connection slave;
}
source include/wait_for_slave_sql_error.inc;

# skip the erroneous statement
if ($slave_skip_counter) {
  eval SET GLOBAL SQL_SLAVE_SKIP_COUNTER= $slave_skip_counter;
}
if (!$slave_skip_counter) {
  SET GLOBAL SQL_SLAVE_SKIP_COUNTER=1;
}
source include/start_slave.inc;
if (!$not_switch_connection)
{
  connection master;
}
