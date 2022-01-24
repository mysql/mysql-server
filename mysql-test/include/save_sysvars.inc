# ==== Purpose ====
#
# The purpose of this utility script is to save the values of given
# configuration variables and prepare the restore of such variables upon
# inclusion of `include/restore_sysvars.inc`.
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
# --let $sysvars_to_save = JSON_ARRAY
# --source include/save_sysvars.inc
#
# Parameters:
#
#   $sysvars_to_save
#     A JSON array with the fully qualified names of the variables to save,
#     for instance:
#
#     let $sysvars_to_save = [
#         "GLOBAL.transaction_write_set_extraction",
#         "GLOBAL.replica_preserve_commit_order",
#         "SESSION.binlog_row_image"
#     ];
#
# Output:
#
#  $sysvar_stack
#    A JSON array containing the SET statements to be executed when
#    `include/restore_sysvars.inc` in included. Each element of the array
#    is a set of SET statements corresponding to a single inclusion of
#    `save_sysvars.inc`. Each invocation to `restore_sysvars.inc` will
#    remove an element of this array.
#
#

if (!$sysvars_to_save) {
  --die ERRROR: `sysvars_to_save` parameter must be set
}

--let $include_filename = save_sysvars.inc $sysvars_to_save
--source include/begin_include_file.inc

--let $json_label = saved_sysvars
--source include/create_json_iterator.inc

--let $sysvars_values_to_save =
--let $json_array = $sysvars_to_save
--source $json_saved_sysvars_start
while (!$json_saved_sysvars_done) {
  --let $sysvar_value = `SELECT @@$json_saved_sysvars_value`
  if ($sysvars_values_to_save != '') {
    --let $sysvars_values_to_save = $sysvars_values_to_save,
  }
  if (`SELECT concat('', @@$json_saved_sysvars_value * 1) != @@$json_saved_sysvars_value`) {
    --let $sysvar_value = '$sysvar_value'
  }
  --let $sysvars_values_to_save = $sysvars_values_to_save @@$json_saved_sysvars_value = $sysvar_value
  --source $json_saved_sysvars_next
}

if ($rpl_debug) {
  --echo DEBUG: sysvars_values_to_save = <SET $sysvars_values_to_save>
}

--let $sysvars_values_to_save = escape(\',$sysvars_values_to_save)
if ($sysvar_stack == '') {
  --let $sysvar_stack = []
}
--let $sysvar_stack = escape(\',$sysvar_stack)
--let $sysvar_stack = `SELECT JSON_ARRAY_INSERT('$sysvar_stack', '$[0]', '$sysvars_values_to_save')`

--source include/end_include_file.inc
