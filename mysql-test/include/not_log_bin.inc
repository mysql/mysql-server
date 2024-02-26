# ==== Purpose ====
#
# Ensure that the server is running with binlogging off
#
# ==== Usage ====
#
# source include/not_log_bin.inc;

--let $_not_log_bin= query_get_value(SHOW GLOBAL VARIABLES LIKE 'log_bin', Value, 1)
if ($_not_log_bin != OFF)
{
  --skip Test requires binlogging to be off.
}

# To avoid warning messages if --binlog-format is used with --skip-log-bin.
--disable_query_log
CALL mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
--enable_query_log

--let $show_rpl_debug_info= 1
