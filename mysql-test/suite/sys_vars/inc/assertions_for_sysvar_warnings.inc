# ==== Purpose ====
#
# Assert that SHOW WARNINGS contains the expected warnings and nothing else.
#
# ==== Usage ====
#
# [--let $expect_deprecation_warning = 1]
# [--let $expect_truncation_warning = 1]
# --source ../inc/assertions_for_sysvar_warnings.inc
#
# Parameters:
#   $expect_deprecation_warning
#     Expect ER_WARN_DEPRECATED_SYNTAX.
#
#   $expect_truncation_warning
#     Expect ER_TRUNCATED_WRONG_VALUE.
#
# ==== Implementation notes ====
#
# Since statements other than SHOW WARNINGS resets the warnings, we
# can't use SQL to check the warnings. So we are limited to mtr
# language, which makes it impossible to use assert.inc, and have to
# use a bit of copy-paste coding.

--let $_csw_expected_warning_count = 0
--let $_csw_expected_warning_desc =
if ($expect_deprecation_warning) {
  --inc $_csw_expected_warning_count
  --let $_csw_expected_warning_desc = $_csw_expected_warning_desc deprecation
  --let $_csw_deprecation_warning_number = convert_error(ER_WARN_DEPRECATED_SYNTAX)
}
if ($expect_truncation_warning) {
  --inc $_csw_expected_warning_count
  --let $_csw_expected_warning_desc = $_csw_expected_warning_desc truncation
  --let $_csw_truncation_warning_number = convert_error(ER_TRUNCATED_WRONG_VALUE)
}

--let $_csw_found_error = 0

--let $_csw_warning_count = `SHOW COUNT(*) WARNINGS`
if ($_csw_warning_count != $_csw_expected_warning_count) {
  --let $_csw_found_error = 1
}

--let $_csw_warning_counter = 1
while ($_csw_warning_counter <= $_csw_warning_count) {
  --let $_csw_warning_error_code = query_get_value("SHOW WARNINGS", Code, $_csw_warning_counter)

  --let $_csw_is_expected_warning = 0
  if ($expect_deprecation_warning) {
    if ($_csw_warning_error_code == $_csw_deprecation_warning_number) {
      --let $_csw_is_expected_warning = 1
    }
  }
  if ($expect_truncation_warning) {
    if ($_csw_warning_error_code == $_csw_truncation_warning_number) {
      --let $_csw_is_expected_warning = 1
    }
  }
  if (!$_csw_is_expected_warning) {
    --let $_csw_found_error = 1
  }

  --inc $_csw_warning_counter
}

if ($_csw_found_error) {
  # Print debug info and fail
  --disable_abort_on_error
  SELECT @@$scope.$variable;
  SELECT @@$scope.$name;
  SELECT @@$scope.$alias;
  SELECT * FROM performance_schema.session_variables WHERE VARIABLE_NAME IN ('$variable', '$name', '$alias');
  SHOW WARNINGS;
  --echo Error: got the $_csw_warning_count warnings above, but expected $_csw_expected_warning_count warnings of types: $_csw_expected_warning_desc
  --die Error: Did not get the expected warnings
}
