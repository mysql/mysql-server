# ==== Requirements ====
#
# For a given scenario specified by the caller, execute the scenario
# and verify the following requirements:
#
# - Replication should not fail.
# - The servers should have the same table data.
# - The servers should write the same to their binary logs, if
#   they use the same setting for binlog_row_image.
# - The servers should have the same binary representations of JSON
#   objects (the actual representation is not accessible through
#   SQL, but we check the sizes using JSON_STORAGE_SIZE).
# - The script also prints decoded rows to the result file so they
#   can be sanity-checked by a human.
#
# ==== Implementation ====
#
# Does the following:
#  1. Execute one or several statements on MASTER
#  2. Replicate to slave_1, slave_2, slave_3, slave_4
#  3. Run mysqlbinlog -v on master. Convert the pseudo-SQL output to
#     real SQL, and apply on 'decoded'. Take the BINLOG statements and
#     apply to 'base64'.
#  4. Run the same mysqlbinlog -v on all servers
#  5. Verify that all mysqlbinlog pseudo-SQL was identical
#  6. Verify that all table data is identical
#  7. Verify that the sizes of JSON objects are identical
#
# ==== Usage ====
#
# --let $master_con= CONNECTION
# --let $slave_1_con= CONNECTION
# --let $slave_2_con= CONNECTION
# --let $slave_3_con= CONNECTION
# --let $slave_4_con= CONNECTION
# --let $slave_5_con= CONNECTION
# --let $base64_con= CONNECTION
# --let $decoded_con= CONNECTION
# [--let $rows= (comma-separated-values)[, (comma-separated-values)[, (...)]]
# [--let $stmt_pre= PREFIX]
# [--let $stmt_post= SUFFIX]
# --let $stmt= STATEMENT
# --let $column_def= DEFINITION
# --let $table= DATABASE.TABLE
# --let $compare_size_columns= COLUMN[, COLUMN[, COLUMN...]]
# [--let $echo_stmt= 1]
# [--let $echo_select= COLUMNS]
# [--let $echo_decoded_rows= 1]
# [--let $first_case= N]
# --source include/check_mysqlbinlog_decoded_rows.inc
#
# Assumptions:
#   Replication should be running from server_1 to server_2.
#   server_3 should be configured but not replicating.
#
# Parameters:
#   $master_con, $slave_[1|2|3|4|5]_con, $base64_con, $decoded_con
#     Connections: statement will originally be executed on
#     $master_con, then replicated to $slave_1_con, $slave_2_con,
#     $slave_3_con, $slave_4_con, and $slave_5_con.
#     mysqlbinlog's base64 output will be applied on $base64_con and
#     the decoded pseudo-SQL will be applied on $decoded_con
#
#   $column_def
#     Whatever should be between the open parenthesis and closing
#     parenthesis in CREATE TABLE (...).  If not given, assumes the
#     invoker has already created the table.
#
#   $stmt
#     The statement(s) to execute.
#
#   $stmt_pre
#     This is prepended to $stmt
#
#   $stmt_post
#     This is appended to $stmt
#
#   $table
#     The qualified name of the table that will be compared on
#     server_1, server_2, and server_3 using include/diff_tables.inc
#
#   $echo_stmt
#     Print the statement before executing it.
#
#   $echo_select
#     If set, issue SELECT $echo_select FROM $table at the beginning
#     and end of the script.  Examples: To select all columns, set
#     $echo_select=*; to select columns 'i' and 'j', set
#     $echo_select=i,j.
#
#   $echo_decoded_rows
#     If set to 1, print the generated SQL statement after decoding it
#     using mysqlbinlog, on master, slave_2, and slave_4 (the output
#     is expected to differ on these, because of the different
#     binlog_row_image).  If set to 2, print it only on master (useful
#     when the full format is too big).
#
#   $compare_size_columns
#     Comma-separated list of names of columns.  The sizes of the listed
#     columns will be compared on all servers.
#
#   $first_case
#     For debugging: If set to a number N>=1, skip all invocations of
#     this script before the Nth.


# Print header
if (!$first_case)
{
  --let $first_case= 1
}
if (!$rpl_apply_decoded_rows_case_counter)
{
  --let $rpl_apply_decoded_rows_case_counter= 0
}
--inc $rpl_apply_decoded_rows_case_counter

if ($rpl_apply_decoded_rows_case_counter >= $first_case)
{

  --echo ---- $rpl_apply_decoded_rows_case_counter. $desc ----

  --disable_query_log

  --let $include_filename= rpl_row_jsondiff_scenario.inc
  --source include/begin_include_file.inc

  # Save binlog positions on all servers
  --let $save_binlog_position_in_sql= 1
  --connection $master_con
  --source include/save_binlog_position.inc
  --connection $slave_1_con
  --source include/save_binlog_position.inc
  --connection $slave_2_con
  --source include/save_binlog_position.inc
  --connection $slave_3_con
  --source include/save_binlog_position.inc
  --connection $slave_4_con
  --source include/save_binlog_position.inc
  --connection $slave_5_con
  --source include/save_binlog_position.inc
  --connection $base64_con
  --source include/save_binlog_position.inc
  --connection $decoded_con
  --source include/save_binlog_position.inc

  if ($column_def)
  {
    if ($column_def != $_last_column_def)
    {
      --echo * CREATE TABLE $table ($column_def)
      --let $rpl_sql= SET SESSION SQL_LOG_BIN = 0; DROP TABLE IF EXISTS $table; CREATE TABLE $table ($column_def); SET SESSION SQL_LOG_BIN = 1;
      --source include/rpl_for_each_server_stmt.inc
    }
  }
  --let $_last_column_def= $column_def

  if ($rows)
  {
    --let $rpl_sql= SET SQL_LOG_BIN = 0; TRUNCATE $table; INSERT INTO $table VALUES $rows; SET SQL_LOG_BIN = 1;
    --source include/rpl_for_each_server_stmt.inc
  }
  if ($echo_select)
  {
    --echo # Before update
    eval SELECT $echo_select FROM $table;
  }

  # Run query on MASTER and sync to slaves
  --connection $master_con
  if ($echo_stmt)
  {
    --echo $stmt_pre $stmt $stmt_post
  }
  eval $stmt_pre $stmt $stmt_post;

  if ($echo_select)
  {
    --echo # After update
    eval SELECT $echo_select FROM $table;
  }

  if ($echo_progress)
  {
    --echo # Syncing to three slaves
  }
  --source include/rpl_sync.inc

  # Prepare for get_mysqlbinlog_decoded_rows
  SET @@SESSION.GROUP_CONCAT_MAX_LEN= 65536;
  --let $columns= `SELECT GROUP_CONCAT(CONCAT(COLUMN_NAME, '/', COLUMN_TYPE) ORDER BY ORDINAL_POSITION SEPARATOR '; ') FROM INFORMATION_SCHEMA.COLUMNS WHERE CONCAT(TABLE_SCHEMA, '.', TABLE_NAME) = '$table'`
  if ($rpl_debug)
  {
    --echo columns=$columns
  }
  --let $mysqlbinlog_positions_from_sql_variables= 1
  --let $mysqlbinlog_options= --force-if-open

  # Get decoded rows on MASTER
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_master= $decoded_sql

  if ($echo_decoded_rows)
  {
    --echo # Decoded rows
    --echo $decoded_sql_master
  }

  # Run converted pseudo-SQL on $decoded_con (unless nothing was logged)
  if ($decoded_sql_master)
  {
    if ($echo_progress)
    {
      --echo # Applying decoded SQL
    }
    --connection $decoded_con
    eval $decoded_sql_master;
  }

  # Run BINLOG base64 on $base64_con
  if ($echo_progress)
  {
    --echo # Applying base64
  }
  --connection $base64_con
  --let $base64_port= `SELECT @@GLOBAL.PORT`
  --exec $MYSQL --user=root --host=127.0.0.1 --port=$base64_port < $output_file
  --remove_file $output_file

  if ($echo_progress)
  {
    --echo # Comparing decoded output
  }

  # Get mysqlbinlog output on all other servers too.

  --connection $slave_1_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_slave_1= $decoded_sql
  --remove_file $output_file

  --connection $slave_2_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_slave_2= $decoded_sql
  --remove_file $output_file

  --connection $slave_3_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_slave_3= $decoded_sql
  --remove_file $output_file

  --connection $slave_4_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_slave_4= $decoded_sql
  --remove_file $output_file

  --connection $slave_5_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_slave_5= $decoded_sql
  --remove_file $output_file

  --connection $base64_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_base64= $decoded_sql
  --remove_file $output_file

  --connection $decoded_con
  --source include/get_mysqlbinlog_decoded_rows.inc
  --let $decoded_sql_decoded= $decoded_sql
  --remove_file $output_file

  if ($echo_decoded_rows == 1)
  {
    --echo # Decoded rows, full image
    --echo $decoded_sql_slave_2

    --echo # Decoded rows, minimal image when master has full image
    --echo $decoded_sql_slave_4
  }

  # Verify that mysqlbinlog has the same output on all servers
  --let $mismatch=

  # When using no primary key, master will generate a full BI but a
  # minimal AI.  Due to BUG#86554, slave will then generate a full AI,
  # so it will be different from the master.  Hence disable comparison
  # with master in the no-pk case.
  if ($key)
  {
    if ($decoded_sql_slave_1 != $decoded_sql_master)
    {
      --let $mismatch= slave_1 vs master;
    }
  }
  # Since slave_2 and slave_3 use binlog_row_image=FULL, they are
  # expected to have different decoded SQL statements from the master,
  # and will only be compared with each other.
  if ($decoded_sql_slave_3 != $decoded_sql_slave_2)
  {
    --let $mismatch= $mismatch slave_3 vs slave_2;
  }
  # Due to BUG#26258329, don't compare slave_4/slave_5 with master,
  # only with each other.
  #if ($decoded_sql_slave_4 != $decoded_sql_master)
  #{
  #  --let $mismatch= slave_4 vs master;
  #}
  if ($decoded_sql_slave_4 != $decoded_sql_slave_5)
  {
    --let $mismatch= $mismatch slave_4 vs slave_5;
  }
  # Must compare with slave_1 and not master due to BUG#86554
  if ($decoded_sql_base64 != $decoded_sql_slave_1)
  {
    --let $mismatch= $mismatch base64 vs slave_1;
  }
  if ($decoded_sql_decoded != $decoded_sql_master)
  {
    --let $mismatch= $mismatch decoded vs master;
  }
  if ($mismatch)
  {
    --source include/show_rpl_debug_info.inc
    --echo FAILURE: output on master: <<<$decoded_sql_master>>>
    --echo FAILURE: output on slave_1: <<<$decoded_sql_slave_1>>>
    --echo FAILURE: output on slave_2: <<<$decoded_sql_slave_2>>>
    --echo FAILURE: output on slave_3: <<<$decoded_sql_slave_3>>>
    --echo FAILURE: output on slave_4: <<<$decoded_sql_slave_4>>>
    --echo FAILURE: output on slave_5: <<<$decoded_sql_slave_5>>>
    --echo FAILURE: output on base64: <<<$decoded_sql_base64>>>
    --echo FAILURE: output on decoded: <<<$decoded_sql_decoded>>>
    --echo FAILURE: The following decoded rows are different: $mismatch
    --echo FAILURE while executing case #$rpl_apply_decoded_rows_case_counter
    --die Decoded rows look different on the different hosts
  }

  # Verify that tables are the same on all servers
  if ($echo_progress)
  {
    --echo # Comparing tables
  }
  --let $diff_tables= $master_con:$table, $slave_1_con:$table, $slave_2_con:$table, $slave_3_con:$table, $slave_4_con:$table, $slave_5_con:$table, $base64_con:$table, $decoded_con:$table
  --source include/diff_tables.inc

  if ($compare_size_columns)
  {
    if ($echo_progress)
    {
      --echo # Comparing sizes of binary representations
    }
    --let $size_query= `SELECT REPLACE('$compare_size_columns', ', ', '), "+", JSON_STORAGE_SIZE(')`
    --let $size_query= SELECT CONCAT(JSON_STORAGE_SIZE($size_query)) FROM $table

    --connection $master_con
    --let $size_master= `$size_query`
    --connection $slave_1_con
    --let $size_slave_1= `$size_query`
    --connection $slave_2_con
    --let $size_slave_2= `$size_query`
    --connection $slave_3_con
    --let $size_slave_3= `$size_query`
    --connection $slave_4_con
    --let $size_slave_4= `$size_query`
    --connection $slave_5_con
    --let $size_slave_5= `$size_query`
    --connection $base64_con
    --let $size_base64= `$size_query`
    --connection $decoded_con
    --let $size_decoded= `$size_query`

    # Compare binary sizes
    --let $mismatch=
    if ($size_slave_1 != $size_master)
    {
      --let $mismatch= slave_1
    }
    if ($size_slave_2 != $size_master)
    {
      --let $mismatch= $mismatch slave_2
    }
    if ($size_slave_3 != $size_master)
    {
      --let $mismatch= $mismatch slave_3
    }
    if ($size_slave_4 != $size_master)
    {
      --let $mismatch= $mismatch slave_4
    }
    if ($size_slave_5 != $size_master)
    {
      --let $mismatch= $mismatch slave_5
    }
    if ($size_base64 != $size_master)
    {
      --let $mismatch= $mismatch base64
    }
    if ($size_decoded != $size_master)
    {
      --let $mismatch= $mismatch decoded
    }
    if ($mismatch)
    {
      --source include/show_rpl_debug_info.inc
      --echo FAILURE: sizes on master: <<<$size_master>>>
      --echo FAILURE: sizes on slave_1: <<<$size_slave_1>>>
      --echo FAILURE: sizes on slave_2: <<<$size_slave_2>>>
      --echo FAILURE: sizes on slave_3: <<<$size_slave_3>>>
      --echo FAILURE: sizes on slave_4: <<<$size_slave_4>>>
      --echo FAILURE: sizes on slave_5: <<<$size_slave_5>>>
      --echo FAILURE: sizes on base64: <<<$size_base64>>>
      --echo FAILURE: sizes on decoded: <<<$size_decoded>>>
      --echo FAILURE: The following are different from master: $mismatch
      --echo FAILURE while executing case #$rpl_apply_decoded_rows_case_counter
      --die Sizes of binary representations are different on the different hosts
    }
  }

  --let $include_filename= rpl_row_jsondiff_scenario.inc
  --source include/end_include_file.inc
}
