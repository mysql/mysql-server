# ==== Purpose ====
#
# Check if the two given tables (possibly residing on different
# master/slave servers) are equal.
#
# ==== Usage ====
#
# The tables to check are given by the test language variables
# $diff_table_1 and $diff_table_2.  They must be of the
# following form:
#
#  [master:|slave:]database.table
#
# I.e., both database and table must be speicified.  Optionally, you
# can prefix the name with 'master:' (to read the table on master) or
# with 'slave:' (to read the table on slave).  If no prefix is given,
# reads the table from the current connection.  If one of these
# variables has a prefix, both should have a prefix.
#
# ==== Side effects ====
#
# - Prints "Comparing tables $diff_table_1 and $diff_tables_2".
#
# - If the tables are different, prints the difference in a
#   system-specific format (unified diff if supported) and generates
#   an error.
#
# - If $diff_table_1 or $diff_table_2 begins with 'master:' or
#   'slave:', it will stay connected to one of those hosts after
#   execution.  The host is only guaranteed to remain unchanged if
#   none of $diff_table_1 or $diff_table_2 begins with 'master:' or
#   'slave:'.
#
# ==== Bugs ====
#
# - It is currently not possible to use this for tables that are
#   supposed to be different, because if the files are different:
#    - 'diff' produces system-dependent output,
#    - the output includes the absolute path of the compared files,
#    - the output includes a timestamp.
#   To fix that, we'd probably have to use SQL to compute the
#   symmetric difference between the tables.  I'm not sure how to do
#   that efficiently.  If we implement this, it would be nice to
#   compare the table definitions too.
#
# - It actually compares the result of "SELECT * FROM table ORDER BY
#   col1, col2, ..., colN INTO OUTFILE 'file'".  Hence, it is assumed
#   that the comparison orders for both tables are equal and that two
#   rows that are equal in the comparison order cannot differ, e.g.,
#   by character case.


# ==== Save both tables to file ====

--echo Comparing tables $diff_table_1 and $diff_table_2
disable_query_log;

--error 0,1
--remove_file $MYSQLTEST_VARDIR/tmp/diff_table_1
--error 0,1
--remove_file $MYSQLTEST_VARDIR/tmp/diff_table_2

let $_diff_table=$diff_table_2;
let $_diff_i=2;
while ($_diff_i) {

  # Parse out any leading "master:" or "slave:" from the table specification
# and connect the appropriate server.
  let $_pos= `SELECT LOCATE(':', '$_diff_table')`;
  let $_diff_conn=`SELECT SUBSTR('$_diff_table', 1, $_pos-1)`;
  if (`SELECT 'XX$_diff_conn' <> 'XX'`) {
    let $_diff_table=`SELECT SUBSTR('$_diff_table', $_pos+1)`;
    connection $_diff_conn;
  }

  # Sanity-check the input.
  let $_diff_error= `SELECT '$_diff_table' NOT LIKE '_%._%'`;
  if ($_diff_error) {
    --echo !!!ERROR IN TEST: \$diff_table_$_diff_i='$_diff_table' is not in the form database.table
    exit;
  }

  # We need the output files to be sorted (so that diff_files does not
  # think the files are different just because they are differently
  # ordered).  To this end, we first generate a query that sorts the
  # table by all columns.  Since ORDER BY accept column indices, we
  # just generate a comma-separated list of all numbers from 1 to the
  # number of columns in the table.
  let $_diff_column_index=`SELECT MAX(ordinal_position)
                           FROM information_schema.columns
                           WHERE CONCAT(table_schema, '.', table_name) =
                                 '$_diff_table'`;
  let $_diff_column_list=$_diff_column_index;
  dec $_diff_column_index;
  while ($_diff_column_index) {
    let $_diff_column_list=$_diff_column_index, $_diff_column_list;
    dec $_diff_column_index;
  }

  # Now that we have the comma-separated list of columns, we can write
  # the table to a file.
  eval SELECT * FROM $_diff_table ORDER BY $_diff_column_list
              INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/diff_table_$_diff_i';

  # Do the same for $diff_table_1.
  dec $_diff_i;
  let $_diff_table=$diff_table_1;
}


# ==== Compare the generated files ====

diff_files $MYSQLTEST_VARDIR/tmp/diff_table_1 $MYSQLTEST_VARDIR/tmp/diff_table_2;

--remove_file $MYSQLTEST_VARDIR/tmp/diff_table_1
--remove_file $MYSQLTEST_VARDIR/tmp/diff_table_2

enable_query_log;
