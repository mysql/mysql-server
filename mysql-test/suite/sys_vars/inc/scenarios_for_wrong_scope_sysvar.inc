# ==== Purpose ====
#
# Verify that a variable that is not defined in a scope cannot be
# accessed in that scope.
#
# ==== Requirements ====
#
# R1. SET should fail in all cases:
#     R1.1. SET @@$scope.$variable = <VALUE>
#     R1.2. SET @@$scope.$variable = DEFAULT
#     R1.3. SET @@$variable = <VALUE>
#     R1.4. SET @@$variable = DEFAULT
#
# R2. SELECT:
#     R2.1. SELECT @@$scope.$variable should fail
#     R2.2. if $scope is global, SELECT @@$variable should fail
#     R2.3. if $scope is session, SELECT @@$variable should succeed
#
# R3. performance_schema:
#     R3.1. if $scope is global, performance_schema.global_variables
#           should not contain the variable
#     R3.1. if $scope is session, performance_schema.session_variables
#           should contain the variable
#
# ==== Usage ====
#
# --let $scope = {global|session}
# --let $name = NAME
# --let $values = JSON_ARRAY
# --let $global = {0|1}
# [--let $mask_value = 1]
# --source scenarios_for_dynamic_sysvar.inc
#
# Parameters:
#
#   $scope
#     The scope to test.
#
#   All other parameters
#     See json_sysvar_spec.inc

--echo ==== Testing SET @@$scope.$name [invalid scope] ====

if (!$dynamic) {
  --let $error_code = ER_INCORRECT_GLOBAL_LOCAL_VAR
}
if ($dynamic) {
  --let $error_code = ER_GLOBAL_VARIABLE
}

--error $error_code
eval SET @@$scope.$name = DEFAULT;
if ($scope == session) {
  --error $error_code
  eval SET @@$name = DEFAULT;
}

# Iterate over values, try to set the variable, and verify that it fails.
--let $json_array = $values
--source $json_value_start
while (!$json_value_done) {

  if ($mask_value) {
    --replace_result $json_value_value VALUE
  }
  --error $error_code
  eval SET @@$scope.$name = $json_value_value;
  if ($scope == session) {
    if ($mask_value) {
      --replace_result $json_value_value VALUE
    }
    --error $error_code
    eval SET @@$name = $json_value_value;
  }

  --source $json_value_next
}

--error ER_INCORRECT_GLOBAL_LOCAL_VAR
eval SELECT @@$scope.$name;
if ($scope == session) {
  if ($mask_value) {
    --disable_result_log
    eval SELECT @@$name;
    --enable_result_log
  }
  if (!$mask_value) {
    eval SELECT @@$name;
  }
}

--let $ps_table_name = _variables
--let $ps_table_name = performance_schema.$scope$ps_table_name
--let $actual_row_count = [SELECT COUNT(*) FROM $ps_table_name WHERE VARIABLE_NAME = '$name']
--let $expected_row_count = 0
--let $exist_text = not exist
if ($scope == session) {
  if ($global) {
    # Global variables always exist in performance_schema.session_variables
    --let $expected_row_count = 1
    --let $exist_text = exist
  }
}

--let $assert_cond = $actual_row_count = $expected_row_count
--let $assert_text = Variable $name should $exist_text in $ps_table_name
--source include/assert.inc
