# ==== Purpose ====
#
# Execute an SQL statement on a new connection and save the result in a table.
# This works even for SQL-unfriendly statements like SHOW.
# It allows you to use perl to filter the result set; for instance,
# select only a subset of the columns.
#
# ==== Limitations ====
#
# This is implemented by executing the command-line client, piping the
# output to a file, running perl scripts on the file, and loading the
# file using LOAD DATA INFILE. As such, it is subject to a few limitations:
# - It does not execute on the current connection. Any session state
#   you expect to use, must be set by the $statement.
# - It will be confused if any result line contains newlines or tabs.
#
# ==== Usage ====
#
# --let $statement = SQL_STATEMENT
# --let $table = [DATABASE.]TABLE
# [--let $table_columns = [(col_name_or_user_var [, col_name_or_user_var] ...)]]
# [--let $script = PERL_SCRIPT]
# [--let $select_columns = NUMBER [, NUMBER] ...]
# [--let $pre_script = PERL_SCRIPT]
# [--let $rpl_debug = 1]
#
# Parameters:
#
#   $statement
#     The SQL statement that will be executed.
#
#   $table
#     Name of table into which the result set should be loaded.
#
#   $table_columns
#     Colums in which values should be inserted.  This will be passed
#     to LOAD DATA INFILE and should have the expected syntactic form
#     for that - see the manual.
#
#   $script, $select_columns, $pre_script
#     The result set will be written to a temporary file, as
#     tab-separated values.  Then, the file is then processed by these
#     scripts, if they are defined.  Finally, the processed file is
#     loaded into the table.  $script and $pre_script are perl
#     scripts, $select_columns is a comma-separated list of columns.
#     See filter_file.inc for details.
#
#  $dont_print_statement
#     Do not echo $statement to the result log.
#
#  $allow_error
#     Do not fail if $statement generates an error.
#
#  $rpl_debug
#     Print extra debug info.

--let $_srit_message = [$table]
if (!$dont_print_statement)
{
  --let $_srit_message = [$statement -> $table]
}

--let $include_filename = include/save_result_in_table.inc $_srit_message
--source include/begin_include_file.inc

--let $output_file = GENERATE
--let $append =
--source include/write_result_to_file.inc

--let $input_file = $output_file
--source include/filter_file.inc

if (!$rpl_debug) {
  --disable_query_log
}

if ($rpl_debug) {
  --echo -------- BEGIN FILTERED FILE --------
  --cat_file $output_file
  --echo -------- END FILTERED FILE --------
}

eval LOAD DATA INFILE '$output_file' INTO TABLE $table
  FIELDS TERMINATED BY '\t' ESCAPED BY '' IGNORE 1 LINES
  $table_columns;

--remove_file $output_file

--let $include_filename = include/save_result_in_table.inc $_srit_message
--source include/end_include_file.inc
