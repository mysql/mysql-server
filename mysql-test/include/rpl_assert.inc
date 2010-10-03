# ==== Purpose ====
#
# Check if a condition holds, fail with debug info if not.
#
# The condition is parsed before executed. The following constructs
# are supported:
#
#   [SQL STATEMENT, COLUMN, ROW]
#     The square bracket is replaced by the result from SQL STATEMENT,
#     in the given COLUMN and ROW.
#
#   <1>
#     This is a shorthand for the result of the first executed square
#     bracket. <2> is a shorthand for the second executed square
#     bracket, and so on.
#
# ==== Usage ====
#
# --let $assert_text= Relay_Log_Pos must be smaller than pos.
# --let $assert_cond= [SHOW SLAVE STATUS, Relay_Log_Pos, 1] >= $min_pos AND <1> <= $max_pos
# [--let $assert_quiet= 1]
# [--let $rpl_debug= 1]
# --source include/rpl_assert.inc
#
# Parameters:
#
# $assert_text
#   Text that describes what is being checked. By default, this text
#   is written to the query log.
#
# $assert_cond
#   Condition to check.  See above for details about the format.  The
#   condition will be executed as `SELECT $assert_cond`.  Note: this
#   condition is parsed using SQL statements, quoted inside single
#   quotes, so it must not contain single quotes itself (use double
#   quotes for strings).
#
# $assert_quiet
#   Do not print $assert_text to the query log.
#
# $rpl_debug
#   Print extra debug info.


if ($rpl_debug)
{
  --echo # debug: assert_text='$assert_text' assert_cond='$assert_cond'
}

# Sanity-check input
if (`SELECT "$assert_text" = ""`)
{
  --die ERROR IN TEST: the mysqltest variable rpl_test must be set
}

# Evaluate square brackets in cond.
--let $_rpl_assert_substmt_number= 1
--let $_rpl_interpolated_cond= $assert_cond
--let $_rpl_assert_lbracket= `SELECT LOCATE('[', '$_rpl_interpolated_cond')`
while ($_rpl_assert_lbracket)
{
  # Get position of right bracket
  --let $_rpl_assert_rbracket= `SELECT LOCATE(']', '$_rpl_interpolated_cond')`
  if (!$_rpl_assert_rbracket)
  {
    --echo BUG IN TEST: Mismatching square brackets in assert_cond: '$assert_cond'
    --die BUG IN TEST: Mismatching square brackets in $assert_cond
  }
  # Get sub-statement and result of it
  --let $_rpl_assert_substmt= `SELECT SUBSTRING('$_rpl_interpolated_cond', $_rpl_assert_lbracket + 1, $_rpl_assert_rbracket - $_rpl_assert_lbracket - 1)`
  --let $_rpl_assert_substmt_result= query_get_value($_rpl_assert_substmt)
  if ($rpl_debug)
  {
    --echo # debug: sub-statement='$_rpl_assert_substmt' result='$rpl_assert_result'
  }
  # Replace sub-statement by its result
  --let $_rpl_interpolated_cond= `SELECT REPLACE('$_rpl_interpolated_cond', '[$_rpl_assert_substmt]', '$_rpl_assert_substmt_result')`
  # Replace result references by result
  --let $_rpl_interpolated_cond= `SELECT REPLACE('$_rpl_interpolated_cond', '<$_rpl_assert_substmt_number>', '$_rpl_assert_substmt_result')`

  --let $_rpl_assert_lbracket= `SELECT LOCATE('[', '$_rpl_interpolated_cond')`

  --inc $_rpl_assert_substmt_number
}

if ($rpl_debug)
{
  --echo # debug: interpolated_cond='$_rpl_interpolated_cond'
}

# Execute.
--let $_rpl_assert_result= `SELECT $_rpl_interpolated_cond`

if ($rpl_debug)
{
  --echo # debug: result='$_rpl_assert_result'
}

# Check.
if (!$_rpl_assert_result)
{
  --echo ######## Test assertion failed: $assert_text ########
  --echo Dumping debug info:
  --source include/show_rpl_debug_info.inc
  --echo Assertion text: '$assert_text'
  --echo Assertion condition: '$assert_cond'
  --echo Assertion condition, interpolated: '$_rpl_interpolated_cond'
  --echo Assertion result: '$_rpl_assert_result'
  --die Test assertion failed in rpl_assertion.inc
}

if (!$assert_quiet)
{
  --echo # Asserted this: $assert_text
}

--let $assert_text=
--let $assert_cond=
