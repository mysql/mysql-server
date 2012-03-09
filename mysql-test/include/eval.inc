# ==== Purpose ====
#
# Evaluate an expression, possibly containing arbitrary
# sub-expressions from different connections.
#
# The expression is parsed before executed. The following constructs
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
#     It is also possible to nest sub-statements on this form, like:
#       [master:SHOW BINLOG EVENTS FROM
#        [slave:SHOW SLAVE STATUS, Master_Log_Pos, 1],
#        1, Position]
#
#   [SQL_STATEMENT]
#     Shortcut to the above form, usable when the result has only one
#     row and one column.
#
#   <1>
#     This is a shorthand for the result of the first executed square
#     bracket. <2> is a shorthand for the second executed square
#     bracket, and so on.
#
# ==== Usage ====
#
# --let $eval_expr= [SHOW SLAVE STATUS, Relay_Log_Pos, 1] + 47
# [--let $eval_no_result= 1]
# [--let $eval_quiet= 1]
# [--let $rpl_debug= 1]
# --source include/eval.inc
# --echo Result was '$eval_result'
#
# Parameters:
#
# $eval_expr
#   Expression to evaluate.  See above for details about the format.  The
#   expression will be executed as `SELECT $eval_expr`.
#
#   Both $eval_expr and the result from any substatement on the
#   form [SQL_STATEMENT, COLUMN, ROW] will be used in SQL statements,
#   inside single quotes (as in '$eval_expr').  So any single quotes
#   in these texts must be escaped or replaced by double quotes.
#
# $eval_no_result
#   By default, the expression is evaluated inside 'SELECT' and the
#   result is stored in $eval_result.  If this variable is set, the
#   expression is instead evaluated as it is and the result is not
#   stored anywhere.
#
# $rpl_debug
#   Print extra debug info.
#
# Return value:
#   The result is stored in $eval_result.

--let $include_filename= eval.inc
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo # debug: eval_expr='$eval_expr' eval_no_result='$eval_no_result'
}

--let $_eval_old_connection= $CURRENT_CONNECTION

# Evaluate square brackets in expr.
--let $_eval_substmt_number= 1
--let $_eval_expr_interp= '$eval_expr'
--let $_eval_rbracket= `SELECT LOCATE(']', $_eval_expr_interp)`
while ($_eval_rbracket)
{
  # Get position of right bracket
  --let $_eval_lbracket= `SELECT $_eval_rbracket - LENGTH(SUBSTRING_INDEX(SUBSTR($_eval_expr_interp, 1, $_eval_rbracket), '[', -1))`
  if ($_eval_lbracket == 0)
  {
    --echo BUG IN TEST: Mismatching square brackets in eval_expr.
    --echo Original eval_expr='$eval_expr'
    --echo Interpolated eval_expr=$_eval_expr_interp
    --die BUG IN TEST: Mismatching square brackets in $eval_expr
  }

  # Get sub-statement from statement. Preserve escapes for single quotes.
  --let $_eval_full_substmt= `SELECT QUOTE(SUBSTRING($_eval_expr_interp, $_eval_lbracket + 1, $_eval_rbracket - $_eval_lbracket - 1))`

  # Get connection from sub-statement
  --let $_eval_colon= `SELECT IF($_eval_full_substmt REGEXP '^[a-zA-Z_][a-zA-Z_0-9]*:', LOCATE(':', $_eval_full_substmt), 0)`
  --let $_eval_connection=
  --let $_eval_substmt= $_eval_full_substmt
  if ($_eval_colon)
  {
    --let $_eval_connection= `SELECT SUBSTRING($_eval_substmt, 1, $_eval_colon - 1)`
    # Preserve escapes for single quotes.
    --let $_eval_substmt= `SELECT QUOTE(SUBSTRING($_eval_substmt, $_eval_colon + 1))`
  }

  # Interpolate escapes before using expression outside string context.
  --let $_eval_substmt_interp= `SELECT $_eval_substmt`

  # Change connection
  if ($_eval_connection)
  {
    if ($rpl_debug)
    {
      --echo # debug: connection='$_eval_connection' sub-statement=$_eval_substmt
    }
    --let $rpl_connection_name= $_eval_connection
    --source include/rpl_connection.inc
  }
  if (!$_eval_connection)
  {
    if ($rpl_debug)
    {
      --echo # debug: old connection, sub-statement=$_eval_substmt
    }
  }

  # Execute and get result from sub-statement.
  # Can't use dollar to denote end of string because mtr will try to
  # interpolate it.
  --let $selected_row_col= `SELECT CONCAT($_eval_substmt, 'ZZENDZZ') REGEXP '[a-zA-Z_][a-zA-Z0-9_]* *, *[0-9][0-9]* *ZZENDZZ'`

  if ($selected_row_col)
  {
    --let $_eval_substmt_result= query_get_value($_eval_substmt_interp)
  }
  if (!$selected_row_col)
  {
    --let $_eval_substmt_result= `$_eval_substmt_interp`
  }

  # Change back connection
  if ($_eval_connection)
  {
    --let $rpl_connection_name= $_eval_old_connection
    --source include/rpl_connection.inc
  }

  if ($rpl_debug)
  {
    --echo # debug: result of sub-statement='$_eval_substmt_result'
  }

  # Replace sub-statement by its result
  --let $_eval_expr_interp= `SELECT QUOTE(REPLACE($_eval_expr_interp, CONCAT('[', $_eval_full_substmt, ']'), '$_eval_substmt_result'))`
  # Replace result references by result
  --let $_eval_expr_interp= `SELECT QUOTE(REPLACE($_eval_expr_interp, '<$_eval_substmt_number>', '$_eval_substmt_result'))`

  --let $_eval_rbracket= `SELECT LOCATE(']', $_eval_expr_interp)`

  --inc $_eval_substmt_number
}

# Interpolate escapes before using expression outside string context.
--let $_eval_expr_interp= `SELECT $_eval_expr_interp`

if ($rpl_debug)
{
  --echo # debug: interpolated_expr='$_eval_expr_interp'
}

# Execute.
if ($eval_no_result)
{
  --eval $_eval_expr_interp
}
if (!$eval_no_result)
{
  --let $eval_result= `SELECT $_eval_expr_interp`
}

if ($rpl_debug)
{
  --echo # debug: result='$eval_result'
}

--let $include_filename= eval.inc
--source include/end_include_file.inc
