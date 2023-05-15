# ==== Purpose ====
#
# Skip a test if a given option was passed on the command line.
#
# ==== Usage ====
#
# --let $variable_names = NAME1[, NAME2 ...]
# --source include/skip_by_command_line.inc
#
# Parameters:
#
# $variable_names
#   Comma-separated list of single-quoted variable names. If any of
#   those global variables were initialized from the command line, the
#   test will be skipped.
#

if ($variable_names == '') {
  --die !!! ERROR IN TEST: set $variable_names
}
if (`SELECT COUNT(*) > 0 FROM performance_schema.variables_info WHERE VARIABLE_NAME IN ($variable_names) AND VARIABLE_SOURCE IN ('COMMAND_LINE')`) {
  --let $failing_variables = `SELECT GROUP_CONCAT(VARIABLE_NAME) FROM performance_schema.variables_info WHERE VARIABLE_NAME IN ($variable_names) AND VARIABLE_SOURCE IN ('COMMAND_LINE')`
  --skip Skipped because the following are set on the command line: $failing_variables
}
