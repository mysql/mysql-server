# ==== Purpose ====
#
# Assert that a given column in SHOW REPLICA STATUS has a given value.
#
# ==== Usage ====
#
# --let $slave_param= COLUMN_NAME
# --let $slave_param_value= VALUE
# [--let $rpl_channel_name= 'NAME']
# [--let $rpl_debug= 1]
# --source include/rpl/assert_replica_status.inc
#
# Parameters:
#   $slave_param, $slave_param_value
#     Column name in output of SHOW REPLICA STATUS that should be checked,
#     and the expected value. Example:
#       --let $slave_param= Exec_Source_Log_Pos
#       --let $slave_param_value= 4711
#
#   $rpl_channel_name
#     If set, add a FOR CHANNEL $rpl_channel_name clause.
#
#   $rpl_debug
#     See include/rpl/init.inc


--let $include_filename= rpl/assert_replica_status.inc [$slave_param]
--let $_csp_for_channel=
if ($rpl_channel_name)
{
  --let $_csp_for_channel= FOR CHANNEL $rpl_channel_name
  --let $include_filename= rpl/assert_replica_status.inc [$slave_param FOR CHANNEL $rpl_channel_name]
}
--source include/begin_include_file.inc


--let $_param_value= query_get_value(SHOW REPLICA STATUS $_csp_for_channel, $slave_param, 1)
if ($_param_value != $slave_param_value) {
  --source include/rpl/debug/show_debug_info.inc
  --echo Wrong value for $slave_param. Expected '$slave_param_value', got '$_param_value'
  --die Wrong value for slave parameter
}


--let $include_filename= rpl/assert_replica_status.inc
--source include/end_include_file.inc
