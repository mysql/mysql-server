# ==== Purpose ====
#
# The purpose of this utility script is to restore the values of given
# configuration variables previously save by `include/save_sysvars.inc`.
#
# IMPORTANT NOTE: The `include/destroy_json_functions.inc` must be included
# by the test including this script (since there is no knowledge if JSON
# functions are being used prior to invoking this script, JSON functions
# can't be cleared within this script). If using replication
# infra-structure, `include/destroy_json_functions.inc` will be already
# included by `include/rpl_end.inc`.
#
# ==== Usage ====
#
# --source include/restore_sysvars.inc
#
if (!$sysvar_stack) {
  --die ERROR: `include/save_sysvars.inc` must be included prior to `include/restore_sysvars.inc`
}

--let $include_filename = restore_sysvars.inc
--source include/begin_include_file.inc

--let $json_label = saved_sysvars_values
--source include/create_json_iterator.inc

--disable_query_log
--let $json_array = $sysvar_stack
--source $json_saved_sysvars_values_start
--eval SET$json_saved_sysvars_values_value
--enable_query_log

--let $sysvar_stack = escape(\',$sysvar_stack)
--let $sysvar_stack = `SELECT JSON_REMOVE('$sysvar_stack', '$[0]')`

if ($sysvar_stack == []) {
  --let $sysvar_stack =
}

--source include/end_include_file.inc
