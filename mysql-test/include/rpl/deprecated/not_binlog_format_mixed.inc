# ==== Purpose ====
#
# Ensure that the server is running without binlog_format MIXED when binlogging
# is ON. And, if binlogging is OFF, don't check for binlog_format.
#
# ==== Usage ====
#
# source include/rpl/deprecated/not_binlog_format_mixed.inc;

--let $_check_log_bin= query_get_value(SHOW GLOBAL VARIABLES LIKE 'log_bin', Value, 1)
if ($_check_log_bin == ON)
{
  if (`SELECT @@binlog_format = 'MIXED'`)
  {
    --skip Test cannot run with binlog_format MIXED
  }
}
