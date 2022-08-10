# ==== Purpose ====
#
# Verify that a readonly variable has the expected default value and
# cannot be set to other values.
#
# ==== Requirements ====
#
# R1. The initial value should be equal to the expected default value.
#
# R2. A specified set of valid values should all result in an error
#     when trying to set them. The value shall be unchagned.
#
# R3. Setting the value to DEFAULT shall fail for the variable.
#
# ==== Usage ====
#
# --let $scope = {session|global}
# --let $name = NAME
# [--let $alias = NAME]
# --let $default = VALUE
# --let $values = JSON_ARRAY
# --source scenarios_for_readonly_sysvar.inc
#
# Parameters:
#
#   $scope
#     The scope to test.
#
#   All other parameters
#     See json_sysvar_spec.inc


--echo ==== Testing SET @@$scope.$name [read-only] ====

--echo # Initial value is default

--let $expect_value = $default
--source assertions_for_sysvar_and_alias_value.inc

--echo # Valid values for *read-only* variable cannot be set
# Iterate over values, set the variable, and verify that it worked.
--let $json_array = $values
--source $json_value_start
while (!$json_value_done) {
  if ($mask_value) {
    --replace_result $json_value_value VALUE
  }
  --error ER_INCORRECT_GLOBAL_LOCAL_VAR
  eval SET @@$scope.$name = $json_value_value;

  --let $expect_value = $default
  --source assertions_for_sysvar_and_alias_value.inc

  --source $json_value_next
}

--echo # Setting default value for variable
--error ER_INCORRECT_GLOBAL_LOCAL_VAR
eval SET @@$scope.$name = DEFAULT;

--let $expect_value = $default
--source assertions_for_sysvar_and_alias_value.inc
