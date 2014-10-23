# ==== Purpose ====
#
# Assert that a given column in SHOW SLAVE STATUS has a given value.
#
# ==== Usage ====
#
# --let $slave_param= COLUMN_NAME
# --let $slave_param_value= VALUE
# [--let $rpl_channel_name = NAME]
# [--let $rpl_debug= 1]
# --source include/check_slave_param.inc
#
# Parameters:
#   $slave_param, $slave_param_value
#     Column name in output of SHOW SLAVE STATUS that should be checked,
#     and the expected value. Example:
#       --let $slave_param= Exec_Master_Log_Pos
#       --let $slave_param_value= 4711
#
#   $rpl_channel_name
#     If set, add a FOR CHANNEL '$rpl_channel_name' clause.
#
#   $rpl_debug
#     See include/rpl_init.inc


--let $_param= $slave_param

--let $_csp_for_channel=
if ($rpl_channel_name != '')
{
  --let $_csp_for_channel= FOR CHANNEL '$rpl_channel_name'
  --let $param= $_param, channel=$rpl_channel_name
}


--let $include_filename= check_slave_param.inc [$_param]
--source include/begin_include_file.inc


--let $_param_value= query_get_value(SHOW SLAVE STATUS $_csp_for_channel, $slave_param, 1)
if ($_param_value != $slave_param_value) {
  --source include/show_rpl_debug_info.inc
  --echo Wrong value for $slave_param. Expected '$slave_param_value', got '$_param_value'
  --die Wrong value for slave parameter
}


--let $include_filename= check_slave_param.inc
--source include/end_include_file.inc
