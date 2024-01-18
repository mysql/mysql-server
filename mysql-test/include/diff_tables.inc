# ==== Purpose ====
#
# Check if all tables in the given list are equal. The tables may have
# different names, exist in different connections, and/or reside in
# different databases.
#
#
# ==== Usage ====
#
# --let $diff_tables= [con1:][db1.]t1, [con2:][db2.]t2, ... , [conN:][dbN.]tN
# [--let $rpl_debug= 1]
# [--let $diff_tables_silent= 1]
# [--let $mask_column_list= c1, .. cn]
# --source include/diff_tables.inc
#
# Parameters:
#   $diff_tables
#     Comma-separated list of tables to compare. Each table has the form
#
#       [CONNECTION:][DATABASE.]table
#
#     If CONNECTION is given, then that connection is used. If
#     CONNECTION is not given, then the connection of the previous
#     table is used (or the current connection, if this is the first
#     table).  If DATABASE is given, the table is read in that
#     database. If DATABASE is not given, the table is read in the
#     connection's current database.
#
#   $diff_tables_silent
#     Do not print table names to result log.
#
#   $rpl_debug
#     See include/rpl/init.inc
#
#
# ==== Side effects ====
#
# - Prints "include/diff_tables.inc [$diff_tables]".
#
# - If the tables are different, prints the difference in a
#   system-specific format (unified diff if supported) and generates
#   an error.
#
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


--let $include_filename= diff_tables.inc
if (!$diff_tables_silent)
{
  --let $include_filename= diff_tables.inc [$diff_tables]
}
--source include/begin_include_file.inc


if (!$rpl_debug)
{
  --disable_query_log
}


# Check sanity
if (`SELECT LOCATE(',', '$diff_tables') = 0`)
{
  --die ERROR IN TEST: $diff_tables must contain at least two tables (separated by comma)
}


# ==== Save both tables to file ====

# Trim off whitespace
--let $_dt_tables= `SELECT REPLACE('$diff_tables', ' ', '')`
--let $mask_column_list= `SELECT REPLACE('$mask_column_list', ' ', '')`

# Add quotes to column names which will be masked.
--let $mask_column_list= `SELECT REPLACE('$mask_column_list', ',', '","')`
if ($mask_column_list) {
  --let $mask_column_clause= AND column_name NOT IN ("$mask_column_list")
}

# Iterate over all tables
--let $_dt_outfile=
--let $_dt_prev_outfile=
while ($_dt_tables)
{
  --let $_dt_table= `SELECT SUBSTRING_INDEX('$_dt_tables', ',', 1)`
  --let $_dt_tables= `SELECT SUBSTRING('$_dt_tables', LENGTH('$_dt_table') + 2)`

  # Parse connection, if any
  --let $_dt_colon_index= `SELECT LOCATE(':', '$_dt_table')`
  if ($_dt_colon_index)
  {
    --let $_dt_connection= `SELECT SUBSTRING('$_dt_table', 1, $_dt_colon_index - 1)`
    --let $_dt_table= `SELECT SUBSTRING('$_dt_table', $_dt_colon_index + 1)`
    --let $rpl_connection_name= $_dt_connection
    --source include/connection.inc
  }

  # Parse database name, if any
  --let $_dt_database_index= `SELECT LOCATE('.', '$_dt_table')`
  if ($_dt_database_index)
  {
    --let $_dt_database= `SELECT SUBSTRING('$_dt_table', 1, $_dt_database_index - 1)`
    --let $_dt_table= `SELECT SUBSTRING('$_dt_table', $_dt_database_index + 1)`
  }
  if (!$_dt_database_index)
  {
    --let $_dt_database= `SELECT DATABASE()`
  }

  if ($rpl_debug)
  {
    --echo con='$_dt_connection' db='$_dt_database' table='$_dt_table'
    --echo rest of tables='$_dt_tables'
  }

  # We need to sort the output files so that diff_files does not think
  # the tables are different just because the rows are differently
  # ordered.  To this end, we first generate a string containing a
  # comma-separated list of all column names. This is used in the
  # ORDER BY clause of the following SELECT statement. We get the
  # column names from INFORMATION_SCHEMA.COLUMNS, and we concatenate
  # them with GROUP_CONCAT. Since GROUP_CONCAT is limited by the
  # @@SESSION.group_concat_max_len, which is only 1024 by default, we
  # first compute the total size of all columns and then increase this
  # limit if needed. We restore the limit afterwards so as not to
  # interfere with the test case.

  # Compute length of ORDER BY clause.
  let $_dt_order_by_length=
    `SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ SUM(LENGTH(column_name) + 3) FROM information_schema.columns
            WHERE table_schema = '$_dt_database' AND
                  table_name = '$_dt_table' AND
                  column_type != 'JSON'
                  $mask_column_clause`;
  if (!$_dt_order_by_length)
  {
    --echo ERROR IN TEST: table $_dt_database.$_dt_table not found in INFORMATION_SCHEMA.COLUMNS. Did you misspell it?
    --die ERROR IN TEST: table not found in INFORMATION_SCHEMA. Did you misspell it?
  }
  --let $_dt_old_group_concat_max_len=
  # Increase group_concat_max_len if needed.
  if (`SELECT $_dt_order_by_length > @@SESSION.group_concat_max_len`)
  {
    --let $_dt_old_group_concat_max_len= `SELECT @@SESSION.group_concat_max_len`
    --eval SET SESSION group_concat_max_len = $_dt_order_by_length;
    if ($rpl_debug)
    {
      --echo # increasing group_concat_max_len from $_dt_old_group_concat_max_len to $_dt_order_by_length
    }
  }
  # Generate ORDER BY clause.
  # It would be better to do GROUP_CONCAT(CONCAT('`', column_name, '`')) but
  # BUG#58087 prevents us from returning strings that begin with backticks.
  let $_dt_column_list=
    `SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ GROUP_CONCAT(column_name ORDER BY ORDINAL_POSITION SEPARATOR '`,`')
            FROM information_schema.columns
            WHERE table_schema = '$_dt_database' AND
                  table_name = '$_dt_table' AND
                  column_type != 'json'
                  $mask_column_clause`;
  # Restore group_concat_max_len.
  if ($_dt_old_group_concat_max_len)
  {
    --let $_dt_dummy= `SET SESSION group_concat_max_len = $_dt_old_group_concat_max_len
  }
  if ($rpl_debug)
  {
    --echo using ORDER BY clause '`$_dt_column_list`'
  }

  # Now that we have the comma-separated list of columns, we can write
  # the table to a file.
  --let $_dt_outfile= `SELECT @@datadir`
  --let $_dt_outfile= $_dt_outfile/diff_table-$_dt_connection-$_dt_database-$_dt_table

  # Certain columns which contain current timestamps can be masked out.
  # $mask_column_list can be set in the test. If it is not defined,
  # all the columns can be written to the file.
  if ($mask_column_list) {
    --let $statement = SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ `$_dt_column_list` FROM $_dt_database.$_dt_table ORDER BY `$_dt_column_list` INTO OUTFILE '$_dt_outfile';
  }
  if (!$mask_column_list) {
    --let $statement = SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM $_dt_database.$_dt_table ORDER BY `$_dt_column_list` INTO OUTFILE '$_dt_outfile';
  }
  if ($rpl_debug)
  {
    --echo $statement
  }
  set @save_sbs = @@sort_buffer_size;
  set @@sort_buffer_size = 1048576;
  --eval $statement
  set @@sort_buffer_size = @save_sbs;

  # Compare files.
  if ($_dt_prev_outfile)
  {
    if ($rpl_debug)
    {
      --echo # diffing $_dt_prev_outfile vs $_dt_outfile
    }
    --diff_files $_dt_prev_outfile $_dt_outfile
    # Remove previous outfile. Keep current file for comparison with next table.
    --remove_file $_dt_prev_outfile
  }
  --let $_dt_prev_outfile= $_dt_outfile
}

--remove_file $_dt_prev_outfile


--let $include_filename= diff_tables.inc [$diff_tables]
--source include/end_include_file.inc
