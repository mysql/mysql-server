# ==== Purpose ====
#
# Verify that a dynamic variable can be set to the expected range of values.
#
# ==== Requirements ====
#
# This verifies the following requirements:
#
# R1. The initial value should be equal to the expected default value.
#
# R2. A specified set of valid values should be settable and result in
#     the expected values.
#
# R3. It is allowed to set the variable to DEFAULT and the result is the
#     expected default value.
#
# R4. If the variable has a minimum, then:
#
#     R4.1. Setting it to the minimum is accepted and results in the
#           minimum value.
#
#     R4.2. If the minimum is not INT64_MIN, setting it to one less than
#           the minimum is accepted and results in the minimum value.
#
#     R4.3. If the block_size is not 1, setting it to block_size less than
#           the minimum is accepted and results in the minimum value.
#
# R5. If the variable has a maximum, then:
#
#     R5.1. Setting it to the maximum is accepted and results in the
#           maximum value.
#
#     R5.2. If the maximum is not INT64_MAX, setting it to one more than
#           the maximum is accepted and results in the maximum value.
#
#     R5.3. If the block_size is not 1, setting it to block_size more than
#           the maximum is accepted and results in the maximum value.
#
# R6. A specified set of invalid values should generate an error when
#     setting them.
#
# R7. If block_size is not 1, setting a value that is not a multiple of
#     the block size rounds it down to the next lower value.
#
# R8. All values checked above are checked both using the @@variable
#     and using the performance_schema table - either
#     performance_schema.global_variabes or
#     performance_schema.session_variables, depending on the scope.
#
# R9. If the variable has an alias, all the values checked above are
#     checked also for the alias.
#
# R10. In all cases above:
#
#     R10.1. A deprecation warning is generated if one is expected,
#            otherwise not.
#
#     R10.2. A truncation warning is generated when using invalid
#            values, otherwise not.
#
# ==== Usage ====
#
# --let $scope = {global|session}
# --let $name = NAME
# [--let $alias = NAME]
# --let $default = VALUE
# --let $values = JSON_ARRAY
# --let $invalid = JSON_ARRAY
# [--let $min = MIN_VALUE]
# [--let $max = MAX_VALUE]
# [--let $block_size = BLOCK_SIZE]
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


--echo ==== Testing SET @@$scope.$name [allowed values, range, invalid values] ====

--echo # Initial value is default

--let $expect_value = $default
--source assertions_for_sysvar_and_alias_value.inc

--echo # Valid values for variable
# Iterate over values, set the variable, and verify that it worked.
--let $json_array = $values
--source $json_value_start
while (!$json_value_done) {
  if ($mask_value) {
    --replace_result $json_value_value VALUE
  }
  eval SET @@$scope.$name=$json_value_value;
  --source assertions_for_sysvar_warnings.inc
  --let $expect_value = $json_value_value
  --source assertions_for_sysvar_and_alias_value.inc

  --source $json_value_next
}

--echo # Setting default value for variable
eval SET @@$scope.$name = DEFAULT;
--source assertions_for_sysvar_warnings.inc
--let $expect_value = $default
--source assertions_for_sysvar_and_alias_value.inc

# Do not mask numeric values, as they are assumed to be deterministic.
# Some numeric values depend on the environment - specifically, debug
# mode on/off or platform word size 32/64 bits.  To test them, you
# should create different versions of the test for the different
# environments.  (Masking the values does not suffice to make the
# result files environment-independent for such variables, since the
# one-more-than maximum case is testable for variables whose maximum
# is INT32MAX but not for variables whose maximum is INT64MAX, so when
# the maximum depends on platform word size, the test takes entirely
# different execution paths.)

--let $_cds_mask_value_save = $mask_value
--let $mask_value = 0

if ($min != '') {
  --echo # Minimum value
  eval SET @@$scope.$name = $min;
  --source assertions_for_sysvar_warnings.inc
  --let $expect_value = $min
  --source assertions_for_sysvar_and_alias_value.inc

  --let $assert_text = Expected min value should equal min value in performance_schema
  --let $assert_cond = [SELECT MIN_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME = '$name'] = $min
  --source include/assert.inc

  --let $minint64 = -18446744073709551616
  # Force mtr to use string comparison instead of numeric comparison,
  # since numeric comparison uses double precision, which may give wrong
  # results for integers of this magnitude.
  --let $xminint64 = x$minint64
  --let $xmin = x$min
  if ($xmin != $xminint64) {
    --echo # Less-than-minimum values
    --let $expect_truncation_warning = 1

    --let $min_minus_1 = `SELECT $min - 1`
    eval SET @@$scope.$name = $min_minus_1;
    --source assertions_for_sysvar_warnings.inc
    --let $expect_value = $min
    --source assertions_for_sysvar_and_alias_value.inc

    if ($block_size != 1) {
      # Can't use mtr's arithmetic since it uses floating-point, which does
      # not have sufficient precision to represent integers of this size.
      --let $value = `SELECT IF($min >= $minint64 + $block_size, $min - $block_size, $minint64)`
      eval SET @@$scope.$name = $value;
      --source assertions_for_sysvar_warnings.inc
      --let $expect_value = $min
      --source assertions_for_sysvar_and_alias_value.inc
    }

    --let $expect_truncation_warning = 0
  }
}

if ($max != '') {
  --echo # Maximum value
  eval SET @@$scope.$name = $max;
  --source assertions_for_sysvar_warnings.inc
  --let $expect_value = $max
  --source assertions_for_sysvar_and_alias_value.inc

  --let $assert_text = Expected max value should equal max value in performance_schema
  --let $assert_cond = [SELECT MAX_VALUE FROM performance_schema.variables_info WHERE VARIABLE_NAME = '$name'] = $max
  --source include/assert.inc

  --let $maxint64 = 18446744073709551615
  --let $xmaxint64 = x$maxint64
  --let $xmax = x$max
  if ($xmax != $xmaxint64) {
    --echo # Greater-than-maximum values

    --let $expect_truncation_warning = 1

    --let $max_plus_1 = `SELECT $max + 1`
    eval SET @@$scope.$name = $max_plus_1;
    --source assertions_for_sysvar_warnings.inc
    --let $expect_value = $max
    --source assertions_for_sysvar_and_alias_value.inc

    if ($block_size != 1) {
      --let $value = `SELECT IF($max <= $maxint64 - $block_size, $max + $block_size, $maxint64)`
      eval SET @@$scope.$name = $value;
      --source assertions_for_sysvar_warnings.inc
      --let $expect_value = $max
      --source assertions_for_sysvar_and_alias_value.inc
    }

    --let $expect_truncation_warning = 0
  }
}

if ($block_size != 1) {
  --let $expect_truncation_warning = 1

  --echo # Non-block-size value
  --let $value = `SELECT $default + $block_size - 1`
  eval SET @@$scope.$name = $value;
  --source assertions_for_sysvar_warnings.inc
  --let $expect_value = $default
  --source assertions_for_sysvar_and_alias_value.inc

  --let $expect_truncation_warning = 0
}

--echo # Invalid values
--let $json_array = $invalid
--source $json_invalid_start
while (!$json_invalid_done) {
  --error ER_WRONG_TYPE_FOR_VAR, ER_WRONG_VALUE_FOR_VAR
  eval SET @@$scope.$name = $json_invalid_value;

  --source $json_invalid_next
}

--echo # Restore default
--let $mask_value = $_cds_mask_value_save

eval SET @@$scope.$name = DEFAULT;
--source assertions_for_sysvar_warnings.inc
--let $expect_value = $default
--source assertions_for_sysvar_and_alias_value.inc
