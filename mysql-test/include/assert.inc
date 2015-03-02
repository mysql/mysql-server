# ==== Purpose ====
#
# Check if a condition holds, fail with debug info if not.
#
# The condition has the same form as expressions evaluated by include/eval.inc
#
# ==== Usage ====
#
# --let $assert_text= Relay_Log_Pos must be between min_pos and max_pos
# --let $assert_cond= [SHOW SLAVE STATUS, Relay_Log_Pos, 1] >= $min_pos AND <1> <= $max_pos
# [--let $extra_debug_info= some text]
# [--let $extra_debug_eval= expression parsable by include/eval.inc]
# [--let $rpl_debug= 1]
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
#   in these texts must be escaped or replaced by double quotes.
#
# $rpl_debug
#   Print extra debug info.
#
# $extra_debug_info, $extra_debug_eval
#   See include/show_rpl_debug_info.inc


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
--source include/eval.inc

# Check.
if (!$eval_result)
{
  --echo ######## Test assertion failed: $assert_text ########
  --echo Dumping debug info:
  if ($show_rpl_debug_info)
  {
    --source include/show_rpl_debug_info.inc
  }
  --echo Assertion text: '$assert_text'
  --echo Assertion condition: '$assert_cond'
  --echo Assertion condition, interpolated: '$_eval_expr_interp'
  --echo Assertion result: '$eval_result'
  --die Test assertion failed in assert.inc
}

--let $include_filename= assert.inc [$assert_text]
--source include/end_include_file.inc

--let $assert_text=
--let $assert_cond=
--let $eval_expr= $_assert_old_eval_expr
--let $eval_result= $_assert_old_eval_result
--let $eval_no_result= $_assert_old_eval_no_result
