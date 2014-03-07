# ==== Purpose ====
# 
# Ensure that the server is running with binlogging on
#
# ==== Usage ====
#
# source include/have_log_bin.inc;

--let $_have_log_bin= query_get_value(SHOW VARIABLES LIKE 'log_bin', Value, 1)
if ($_have_log_bin != ON)
{
  --skip Test requires --log-bin
}
--let $show_rpl_debug_info= 1
