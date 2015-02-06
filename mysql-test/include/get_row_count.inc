# ==== Purpose ====
#
# Execute a statement and compute the number of rows of the result set.
#
# This is intended to be used for queries like SHOW BINLOG EVENTS
# where you cannot use COUNT(*). It would be nicer if mysqltest had
# this feature. Now we have to execute the statement several times and
# see if 'query_get_value(*, row) returns 'No such row'. Beware to not
# use this for queries that have side effects.
#
# ==== Usage ====
#
# --let $statement= STATEMENT
# --let $column= COLUMN_NAME
# --source include/get_row_count.inc
# --echo $statement has $row_count rows
#
# Parameters:
#   $statement
#     The statement to execute. Beware that this will be executed
#     multiple times.
#
#   $column
#     The name of one column of the result. This may seem redundant
#     but unfortunately mysqltest forces you to specify a column name.
#
# Return value:
#   $row_count
#     The number of rows is stored in this variable.

# Find upper bound on number of rows: check row numbers 1, 2, 4, 8, etc
--let $_grc_max_row= 0
--let $_grc_min_row= 0
--let $_grc_row= 1
while ($_grc_max_row == 0)
{
  if ($rpl_debug)
  {
    --echo row=$_grc_row min_row=$_grc_min_row max_row=$_grc_max_row
  }
  --let $_grc_result= query_get_value($statement, $column, $_grc_row)
  if ($_grc_result == 'No such row')
  {
    --let $_grc_max_row= $_grc_row
  }
  if ($_grc_result != 'No such row')
  {
    --let $_grc_min_row= $_grc_row
    --let $_grc_row= `SELECT $_grc_row * 2`
  }
}

# Binary search to find exact count.
while (`SELECT $_grc_min_row + 1 < $_grc_max_row`)
{
  if ($rpl_debug)
  {
    --echo row=$_grc_row min_row=$_grc_min_row max_row=$_grc_max_row
  }
  --let $_grc_row= `SELECT ($_grc_min_row + $_grc_max_row) DIV 2`
  --let $_grc_result= query_get_value($statement, $column, $_grc_row)
  if ($_grc_result == 'No such row')
  {
    --let $_grc_max_row= $_grc_row
  }
  if ($_grc_result != 'No such row')
  {
    --let $_grc_min_row= $_grc_row
  }
}

# Store result
--let $row_count= $_grc_min_row
