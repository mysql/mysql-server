# ==== Purpose ====
#
# Check if a condition holds, fail with debug info if not.
#
# The condition is parsed before executed. The following constructs
# are supported:
#
#   [SQL_STATEMENT, COLUMN, ROW]
#     The square bracket is replaced by the result from SQL_STATEMENT,
#     in the given COLUMN and ROW.
#
#     Optionally, SQL_STATEMENT may have the form:
#       connection:SQL_STATEMENT
#     In this case, SQL_STATEMENT is executed on the named connection.
#     All other queries executed by this script will be executed on
#     the connection that was in use when this script was started.
#     The current connection will also be restored at the end of this
#     script.
#
#     Nested sub-statements on this form are not allowed.
#
#   <1>
#     This is a shorthand for the result of the first executed square
#     bracket. <2> is a shorthand for the second executed square
#     bracket, and so on.
#
# ==== Usage ====
#
# --let $assert_text= Relay_Log_Pos must be between min_pos and max_pos
# --let $assert_cond= [SHOW SLAVE STATUS, Relay_Log_Pos, 1] >= $min_pos AND <1> <= $max_pos
# [--let $assert_quiet= 1]
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


--let $include_filename= assert.inc [$assert_text]
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo # debug: assert_text='$assert_text' assert_cond='$assert_cond'
}

# Sanity-check input
if (!$assert_text)
{
  --die ERROR IN TEST: the mysqltest variable rpl_test must be set
}

--let $_assert_old_connection= $CURRENT_CONNECTION

# Evaluate square brackets in cond.
--let $_assert_substmt_number= 1
--let $_assert_cond_interp= '$assert_cond'
--let $_assert_lbracket= `SELECT LOCATE('[', $_assert_cond_interp)`
while ($_assert_lbracket)
{
  # Get position of right bracket
  --let $_assert_rbracket= `SELECT LOCATE(']', $_assert_cond_interp)`
  if (!$_assert_rbracket)
  {
    --echo BUG IN TEST: Mismatching square brackets in assert_cond.
    --echo Original assert_cond='$assert_cond'
    --echo Interpolated assert_cond=$_assert_cond_interp
    --die BUG IN TEST: Mismatching square brackets in $assert_cond
  }

  # Get sub-statement from statement. Preserve escapes for single quotes.
  --let $_assert_full_substmt= `SELECT QUOTE(SUBSTRING($_assert_cond_interp, $_assert_lbracket + 1, $_assert_rbracket - $_assert_lbracket - 1))`

  # Get connection from sub-statement
  --let $_assert_colon= `SELECT IF($_assert_full_substmt REGEXP '^[a-zA-Z_][a-zA-Z_0-9]*:', LOCATE(':', $_assert_full_substmt), 0)`
  --let $_assert_connection=
  --let $_assert_substmt= $_assert_full_substmt
  if ($_assert_colon)
  {
    --let $_assert_connection= `SELECT SUBSTRING($_assert_substmt, 1, $_assert_colon - 1)`
    # Preserve escapes for single quotes.
    --let $_assert_substmt= `SELECT QUOTE(SUBSTRING($_assert_substmt, $_assert_colon + 1))`
  }

  # Interpolate escapes before using condition outside string context.
  --let $_assert_substmt_interp= `SELECT $_assert_substmt`

  # Execute and get result from sub-statement
  if ($_assert_connection)
  {
    if ($rpl_debug)
    {
      --echo # debug: connection='$_assert_connection' sub-statement=$_assert_substmt
    }
    --let $rpl_connection_name= $_assert_connection
    --source include/rpl_connection.inc
    --let $_assert_substmt_result= query_get_value($_assert_substmt_interp)
    --let $rpl_connection_name= $_assert_old_connection
    --source include/rpl_connection.inc
  }
  if (!$_assert_connection)
  {
    if ($rpl_debug)
    {
      --echo # debug: old connection, sub-statement=$_assert_substmt
    }
    --let $_assert_substmt_result= query_get_value($_assert_substmt_interp)
  }
  if ($rpl_debug)
  {
    --echo # debug: result of sub-statement='$_assert_substmt_result'
  }

  # Replace sub-statement by its result
  --let $_assert_cond_interp= `SELECT QUOTE(REPLACE($_assert_cond_interp, CONCAT('[', $_assert_full_substmt, ']'), '$_assert_substmt_result'))`
  # Replace result references by result
  --let $_assert_cond_interp= `SELECT QUOTE(REPLACE($_assert_cond_interp, '<$_assert_substmt_number>', '$_assert_substmt_result'))`

  --let $_assert_lbracket= `SELECT LOCATE('[', $_assert_cond_interp)`

  --inc $_assert_substmt_number
}

# Interpolate escapes before using condition outside string context.
--let $_assert_cond_interp= `SELECT $_assert_cond_interp`

if ($rpl_debug)
{
  --echo # debug: interpolated_cond='$_assert_cond_interp'
}

# Execute.
--let $_assert_result= `SELECT $_assert_cond_interp`

if ($rpl_debug)
{
  --echo # debug: result='$_assert_result'
}

# Check.
if (!$_assert_result)
{
  --echo ######## Test assertion failed: $assert_text ########
  --echo Dumping debug info:
  if ($rpl_inited)
  {
    --source include/show_rpl_debug_info.inc
  }
  --echo Assertion text: '$assert_text'
  --echo Assertion condition: '$assert_cond'
  --echo Assertion condition, interpolated: '$_assert_cond_interp'
  --echo Assertion result: '$_assert_result'
  --die Test assertion failed in assertion.inc
}

--let $include_filename= assert.inc [$assert_text]
--source include/end_include_file.inc

--let $assert_text=
--let $assert_cond=
