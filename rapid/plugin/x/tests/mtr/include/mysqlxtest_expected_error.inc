## Script mysqlxtest_expected_error.inc
#
## Usage
#
# [--let $expected_error_line = number of error line]
# --let $expected_error_msg = text of expected message
# [--let $expected_error_extra = extra regex string(s) for errors suppresion]
# --source ../include/mysqlxtest_expected_error.inc
#
if ($expected_error_msg != '')
{
  if ($expected_error_line == '')
  {
    --let $expected_error_line= 0
  }
  --let $expected_error_regex= /in main, line $expected_error_line:ERROR: ($expected_error_msg)/Application terminated with expected error: \1/ $expected_error_extra
  --let $expected_error_line=
  --let $expected_error_msg=
  --let $expected_error_extra=
  --replace_regex $expected_error_regex
  --error 1
}
