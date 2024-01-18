# ==== Purpose ====
#
# Check if a condition holds, fail with debug info if not.
#
# The condition has the same form as expressions evaluated by include/eval.inc
#
# ==== Usage ====
#
# --let $assert_text = Relay_Log_Pos must be between min_pos and max_pos
# --let $assert_cond = [SHOW REPLICA STATUS, Relay_Log_Pos, 1] >= $min_pos AND <1> <= $max_pos
# [--let $assert_escape = 1]
# [--let $extra_debug_info = some text]
# [--let $extra_debug_eval = expression parsable by include/eval.inc]
# [--let $rpl_debug = 1]
# --source include/assert.inc
#
# Parameters:
#
# $assert_text
#   Text that describes what is being checked. This text is written to
#   the query log so it should not contain non-deterministic elements.
#
# $assert_cond
#   Condition to check.  See above for details about the format.  The
#   condition will be executed as `SELECT $assert_cond`.
#
#   Both $assert_cond and the result from any substatement on the
#   form [SQL_STATEMENT, COLUMN, ROW] will be used in SQL statements,
#   inside single quotes (as in '$assert_text').  So any single quotes
#   in these texts must be escaped or replaced by double quotes, unless
#   $assert_escape is used.
#
# $assert_escape
#   By default, this script passes $assert_cond within single quotes
#   to the server in SQL statements.  Therefore, any quotes or
#   backslashes appearing in $eval_expr must be escaped.  If
#   $assert_escape==1, this script escapes any single quotes that
#   appear in $assert_cond, so the caller does not have to know how
#   this script processes quotes.
#
# $rpl_debug
#   Print extra debug info.
#
# $assert_no_stop
#   Do not make the test fail even if the assert fails.  This is
#   sometimes preferrable when debugging a script, since it may allow
#   several errors to be caught in a single run.
#
# $extra_debug_info, $extra_debug_eval
#   See include/rpl/debug/show_debug_info.inc


--let $include_filename= assert.inc [$assert_text]
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo # debug: assert_text='$assert_text' assert_cond='$assert_cond'
}

# Sanity-check input
if (!$assert_text)
{
  --die ERROR IN TEST: the mysqltest variable assert_text must be set
}

--let $_assert_old_eval_expr= $eval_expr
--let $_assert_old_eval_result= $eval_result
--let $_assert_old_eval_no_result= $eval_no_result
--let $eval_expr= $assert_cond
--let $eval_escape = $assert_escape
--source include/eval.inc

# Check.
if (!$eval_result)
{
  --echo ######## Test assertion failed: $assert_text ########
  --echo Dumping debug info:
  --let $assert_cond_interp = $_eval_expr_interp
  --let $assert_result = $eval_result
  if ($show_rpl_debug_info)
  {
    --source include/rpl/debug/show_debug_info.inc
  }
  if ($assert_no_stop) {
    if (!$show_rpl_debug_info) {
      --echo DO_NOT_CHECK_IN_THIS_LINE: When invoking include/assert.inc, assert_no_stop should only be used for debugging. Never check in a test that uses it to succeed.
    }
  }

  --echo Assertion text: '$assert_text'
  --echo Assertion condition: '$assert_cond'
  --echo Assertion condition, interpolated: '$assert_cond_interp'
  --echo Assertion result: '$assert_result'
  if (!$assert_no_stop) {
    --die Test assertion failed in assert.inc
  }
}

--let $include_filename= assert.inc [$assert_text]
--source include/end_include_file.inc

--let $assert_text=
--let $assert_cond=
--let $eval_expr= $_assert_old_eval_expr
--let $eval_result= $_assert_old_eval_result
--let $eval_no_result= $_assert_old_eval_no_result
