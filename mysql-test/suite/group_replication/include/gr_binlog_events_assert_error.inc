# ==== Purpose ====
#
# This script which is part of gr_binlog_events_verifications verifies
# that events obeys following GR requirements:
# - tables having no primary keys
# - tables using non-innodb storage engine
# - tables with foreign key with cascade clause
#
# ==== Usage ====
#
# --let $slave_sql_errno= NUMBER
# --let $slave_sql_error= TEXT
# --let $gr_test_name= NAME
# [--let $rpl_mysqlbinlog= 1]
# [--let $rpl_slave_connection_name= <connection_name>]
# [--let $rpl_server_port= <server_port>]
# [--let $rpl_start_binlog_pos= NUMBER]
# --let $rpl_curr_gtid= UUID:ID
# --let $rpl_error_log= FILE
# --let $slave_sql_error= TEXT
# [--let $rpl_gr_grep_primary_error= [0|1]]
# [--let $rpl_gr_grep_innodb_error= [0|1]]
# [--let $rpl_gr_grep_foreign_cascade_error= [0|1]]
# [--let $rpl_gr_grep_primary_error_message= TEXT]
# [--let $rpl_gr_grep_innodb_error_message= TEXT]
# [--let =$rpl_gr_grep_foreign_cascade_error_message= TEXT]
# --source ../extra/gr_binlog_events_assert_error.test
#
#
# Parameters:
#
#   $slave_sql_errno
#     The expected SQL error number.
#
#   $slave_sql_error
#     The expected SQL error message.
#
#   $gr_test_name
#     The current test name getting executed.
#
#   $rpl_mysqlbinlog
#     If not set the binlog events are applied through asynchronous
#     replication otherwise through mysqlbinlog.
#
#   $rpl_slave_connection_name
#     The server connection name to which the binlog events are to
#     be applied.
#
#   $rpl_server_port
#     The server port number to which the binlog events are to
#     be applied. This variable is only required when $rpl_mysqlbinlog=1
#     i.e. when binlog events are to be applied using mysqlbinlog
#     utility.
#
#   $rpl_start_binlog_pos
#     The server port number to which the binlog events are to
#     be applied. This variable is only required when $rpl_mysqlbinlog=1
#     i.e. when binlog events are to be applied using mysqlbinlog
#     utility.
#
#   $rpl_curr_gtid
#     The current gtid for which error message has been logged in error log.
#
#   $rpl_error_log
#     The error log file to search.
#
#   $rpl_gr_grep_primary_error_message
#     The error message which is written to error log when table
#     doesn't have primary key.
#
#   $rpl_gr_grep_innodb_error_message
#     The error message which is written to error log when table
#     is of non-innodb storage engine.
#
#   $rpl_gr_grep_foreign_cascade_error_message
#     The error message which is written to error log when table
#     has foreign key cascade clause.
#
#   $rpl_gr_grep_primary_error
#     If set will search for $rpl_gr_grep_primary_error_message in error log.
#
#   $rpl_gr_grep_innodb_error
#     If set will search for $rpl_gr_grep_innodb_error_message in error log.
#
#   $rpl_gr_grep_foreign_cascade_error
#     If set will search for $rpl_gr_grep_foreign_cascade_error_message in
#     error log.
#
################################################################################

if (!$gr_test_name)
{
  --die ERROR IN TEST: You must set $gr_test_name before sourcing gr_binlog_events_assert_error.inc
}

if (!$slave_sql_errno)
{
  --die ERROR IN TEST: You must set $slave_sql_errno before sourcing gr_binlog_events_assert_error.inc
}

if (!$slave_sql_error)
{
  --die ERROR IN TEST: You must set $slave_sql_error before sourcing gr_binlog_events_assert_error.inc
}

if (!$rpl_error_log)
{
  --die ERROR IN TEST: You must set $rpl_error_log before sourcing gr_binlog_events_assert_error.inc
}

if (`SELECT "$rpl_gr_grep_primary_error" = "0" and "$rpl_gr_grep_innodb_error" = "0" and "$rpl_gr_grep_foreign_cascade_error" = "0"`)
{
  --die ERROR IN TEST: You must set either of rpl_gr_grep_primary_error or rpl_gr_grep_innodb_error or rpl_gr_grep_foreign_cascade_error before sourcing gr_binlog_events_assert_error.inc
}

# wait for slave channel to error out
if (!$rpl_mysqlbinlog)
{
  --let $rpl_connection_name= $rpl_slave_connection_name
  --source include/rpl_connection.inc

  --let $rpl_skip_gtid= $rpl_curr_gtid
  --source include/wait_for_slave_sql_error_and_restart.inc
}

# apply binlog events using mysqlbinlog and expect error
if ($rpl_mysqlbinlog)
{
  # Save mysqlbinlog output in a file
  --let $_rpl_server_datadir= `SELECT @@DATADIR`
  --let $_rpl_server_binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)

  --exec $MYSQL_BINLOG --start-position=$rpl_start_binlog_pos --force-if-open $_rpl_server_datadir/$_rpl_server_binlog_file > $rpl_out_binlog

  # save binlog position
  --let $rpl_start_binlog_pos= query_get_value(SHOW MASTER STATUS, Position, 1)

  # Execute mysqlbinlog output of server2 on server1. Expect error.
  --let $rpl_connection_name= $rpl_slave_connection_name
  --source include/rpl_connection.inc

  if (!$rpl_server_port)
  {
    --let $rpl_server_port= `SELECT @@PORT`
  }

  --let $_rpl_server_connection_args= --user=root --host=127.0.0.1 --port=$rpl_server_port
  --error 1
  --exec $MYSQL $_rpl_server_connection_args -e "source $rpl_out_binlog"
}

# assert primary key error message in error log
if ($rpl_gr_grep_primary_error)
{
  --let $grep_file= $rpl_error_log
  --let $grep_pattern=.* \[ERROR\].*$rpl_gr_grep_primary_error_message.*
  --let $grep_output= boolean
  --source include/grep_pattern.inc

  --let $rpl_gr_grep_primary_error= 0
}

# assert non-innodb table found error message in error log
if ($rpl_gr_grep_innodb_error)
{
  --let $grep_file= $rpl_error_log
  --let $grep_pattern=.* \[ERROR\].*$rpl_gr_grep_innodb_error_message.*
  --let $grep_output= boolean
  --source include/grep_pattern.inc

  --let $rpl_gr_grep_innodb_error= 0
}

# assert foreign key with cascade clause found error message in error log
if ($rpl_gr_grep_foreign_cascade_error)
{
  --let $grep_file= $rpl_error_log
  --let $grep_pattern=.* \[ERROR\].*$rpl_gr_grep_foreign_cascade_error_message.*
  --let $grep_output= boolean
  --source include/grep_pattern.inc

  --let $rpl_gr_grep_foreign_cascade_error= 0
}

# assert for events applied through asynchronous replication
# as events applied through mysqlbinlog won't have these errors
if (!$rpl_mysqlbinlog)
{
  --let $_tmp_assert_select = `SELECT REPLACE('$slave_sql_error', '.*:.*', $rpl_curr_gtid)`

  --let $grep_file= $rpl_error_log
  --let $grep_pattern=.* \[ERROR\].*$_tmp_assert_select.*
  --let $grep_output= print_count
  --source include/grep_pattern.inc
}
