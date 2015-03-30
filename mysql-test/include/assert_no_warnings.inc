# ==== Purpose ====
#
# Asserts that the current session has no warnings or threw only
# expected warnings.
#
# ==== Usage ====
#
# [--let $assert_no_warnings_exception= NUMBER ]
#  or
# [--let $assert_no_warnings_exception= convert_error(ER_...) ]
# --source include/assert_no_warnings.inc
#
# Parameters:
#
#   $assert_no_warnings_exception
#     The error code of a possible expected warning to avoid failing this
#     assert on expected warnings.
#

--let $_warning_rows= `SHOW COUNT(*) WARNINGS`
--let $_warning_counter= 1

while ( $_warning_counter <= $_warning_rows )
{
  --let $_warning_error_code= query_get_value("SHOW WARNINGS", Code, $_warning_counter)
  if ( $_warning_error_code != $assert_no_warnings_exception )
  {
    if ($assert_no_warnings_exception)
    {
      --echo assert_no_warnings.inc exception : $assert_no_warnings_exception
    }
    SHOW WARNINGS;
    --die assert_no_warnings.inc has found a unexpected warning
  }
  --inc $_warning_counter
}
