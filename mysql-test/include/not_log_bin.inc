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
  --skip Test requires binloggin to be off.
}
--let $show_rpl_debug_info= 1
