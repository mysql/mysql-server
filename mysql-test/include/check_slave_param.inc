# ==== Purpose ====
#
# Assert that a given column in SHOW SLAVE STATUS has a given value.
#
# ==== Usage ====
#
# --let $slave_param= Exec_Master_Log_Pos
# --let $slave_param_value= 4711
# --source include/check_slave_param.inc

--let $_param_value= query_get_value(SHOW SLAVE STATUS, $slave_param, 1)
if (`SELECT '$_param_value' != '$slave_param_value'`) {
  --echo Wrong value for $slave_param. Expected '$slave_param_value', got '$_param_value'
  --source include/show_rpl_debug_info.inc
  --die Wrong value for slave parameter
}
