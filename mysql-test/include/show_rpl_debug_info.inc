# ==== Purpose ====
#
# Print status information for replication, typically used to debug
# test failures.
#
# First, the following is printed on slave:
#
#   SHOW SLAVE STATUS
#   SHOW PROCESSLIST
#   SHOW BINLOG EVENTS IN <binlog_name>
#
# Where <binlog_name> is the currently active binlog.
#
# Then, the following is printed on master:
#
#   SHOW MASTER STATUS
#   SHOW PROCESSLIST
#   SHOW BINLOG EVENTS IN <sql_binlog_name>
#   SHOW BINLOG EVENTS IN <io_binlog_name>
#
# Where <sql_binlog_name> is the binlog name that the slave sql thread
# is currently reading from and <io_binlog_name> is the binlog that
# the slave IO thread is currently reading from.
#
# ==== Usage ====
#
# [let $master_connection= <connection>;]
# source include/show_rpl_debug_info.inc;
#
# If $master_connection is set, debug info will be retrieved from the
# connection named $master_connection.  Otherwise, it will be
# retrieved from the 'master' connection if the current connection is
# 'slave'.

let $_con= $CURRENT_CONNECTION;
--echo
--echo [on $_con]
--echo
SELECT NOW();
--echo **** SHOW SLAVE STATUS on $_con ****
query_vertical SHOW SLAVE STATUS;
--echo
--echo **** SHOW PROCESSLIST on $_con ****
SHOW PROCESSLIST;
--echo
--echo **** SHOW BINLOG EVENTS on $_con ****
let $binlog_name= query_get_value("SHOW MASTER STATUS", File, 1);
eval SHOW BINLOG EVENTS IN '$binlog_name';

let $_master_con= $master_connection;
if (!$_master_con)
{
  if (`SELECT '$_con' = 'slave'`)
  {
    let $_master_con= master;
  }
  if (!$_master_con)
  {
    --echo Unable to determine master connection. No debug info printed for master.
    --echo Please fix the test case by setting $master_connection before sourcing
    --echo show_rpl_debug_info.inc.
  }
}

if ($_master_con)
{

  let $master_binlog_name_io= query_get_value("SHOW SLAVE STATUS", Master_Log_File, 1);
  let $master_binlog_name_sql= query_get_value("SHOW SLAVE STATUS", Relay_Master_Log_File, 1);
  --echo
  --echo [on $_master_con]
  connection $_master_con;
  --echo
  SELECT NOW();
  --echo **** SHOW MASTER STATUS on $_master_con ****
  query_vertical SHOW MASTER STATUS;
  --echo
  --echo **** SHOW PROCESSLIST on $_master_con ****
  SHOW PROCESSLIST;
  --echo
  --echo **** SHOW BINLOG EVENTS on $_master_con ****
  eval SHOW BINLOG EVENTS IN '$master_binlog_name_sql';
  if (`SELECT '$master_binlog_name_io' != '$master_binlog_name_sql'`)
  {
    eval SHOW BINLOG EVENTS IN '$master_binlog_name_io';
  }

  connection $_con;
}
