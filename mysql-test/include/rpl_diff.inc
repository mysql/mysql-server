# ==== Purpose ====
#
# Diff the output of a statement on all configured servers (usually
# master and slave).
#
#
# ==== Usage =====
#
# --let $rpl_diff_statement= SELECT * FROM t1 WHERE a < 100
# [--let $rpl_diff_servers= <server1>,<server2>,...<serverN>]
# [--let $rpl_diff_database= database]
# [--let $rpl_skip_sync= 1]
# [--let $rpl_only_running_threads= 1]
# --source include/rpl_diff.inc
#
# Parameters:
#   $rpl_diff_statement
#     Statement to check. For each compared server, this script will
#     start a new client and pass this statement to the client.
#     Note: This string will be evaluated as a single-quote-escaped
#     SQL string and hence must be quoted as such.  In particular, any
#     single quotes in this string must be escaped.
#
#   $rpl_diff_servers
#     By default, this file compares all servers configured by
#     rpl_init.inc.  You can set $diff_servers to a comma-separated
#     list of numbers: only the servers identified by these numbers
#     will be compared.
#
#   $rpl_diff_database
#     By default, the statement will be executed on the database
#     'test'.  If $rpl_diff_database is set, the statement will be
#     executed on the database named $rpl_diff_database instead.
#
#   $rpl_skip_sync
#     By default, all slaves are synced using rpl_sync.inc. Set this
#     option to 1 to disable this behavior (note that you must
#     manually sync all servers in this case). Normally you want to
#     sync, but you need to disable sync if you use multi-source.
#
#   $rpl_only_running_threads
#     If one or both of the IO and SQL threads is stopped, sync and
#     stop only the threads that are running.  See
#     include/rpl_sync.inc and include/stop_slave.inc for details.


--let $include_filename= rpl_diff.inc
--source include/begin_include_file.inc


if (!$rpl_diff_statement)
{
  --die ERROR IN TEST: you must set $rpl_diff_statement before you source include/rpl_diff.inc
}


# Sync.
if (!$rpl_skip_sync)
{
  --source include/rpl_sync.inc
}


# Get database name.
--let $_rpl_diff_database= $rpl_diff_database
if (!$_rpl_diff_database)
{
  --let $_rpl_diff_database= test
}


# Generate list of servers.
--let $_rpl_diff_servers= $rpl_diff_servers
if (!$_rpl_diff_servers)
{
  --let $_rpl_server_i= $rpl_server_count
  --let $_rpl_diff_servers= 
  while ($_rpl_server_i)
  {
    --let $_rpl_diff_servers= $_rpl_server_i,$_rpl_diff_servers
    --dec $_rpl_server_i
  }
}
if ($rpl_debug)
{
  --echo \$rpl_diff_servers= '$_rpl_diff_servers'
}


if (!$rpl_debug)
{
  --disable_query_log
}


# Generate file containing $rpl_diff_statement. We don't pass the
# statement on the command line, because it would be subject to shell
# substitutions.
--let $write_to_file= GENERATE
--let $write_var= $rpl_diff_statement
--source include/write_var_to_file.inc
--let $_rpl_diff_statement_file= $write_to_file


# Compare all servers.
--let $_rpl_diff_first= 1
while ($_rpl_diff_servers)
{
  # Set $_rpl_diff_server_i to the first number in the list
  --let $_rpl_diff_server_i= `SELECT SUBSTRING_INDEX('$_rpl_diff_servers', ',', 1)`
  # Remove $_rpl_diff_server_i from the list
  --let $_rpl_diff_servers= `SELECT SUBSTRING('$_rpl_diff_servers', LENGTH('$_rpl_diff_server_i') + 2)`

  # Execute statement
  --let $_rpl_diff_file= $MYSQLTEST_VARDIR/tmp/_rpl_diff_server-$_rpl_diff_server_i.tmp
  --exec $MYSQL --defaults-group-suffix=.$_rpl_diff_server_i $_rpl_diff_database < $_rpl_diff_statement_file > $_rpl_diff_file

  # Compare
  if (!$_rpl_diff_first)
  {
    if ($rpl_debug)
    {
      --echo diffing $_rpl_diff_file and $_rpl_diff_prev_file
    }
    --diff_files $_rpl_diff_file $_rpl_diff_prev_file
    --remove_file $_rpl_diff_prev_file
  }
  --let $_rpl_diff_prev_file= $_rpl_diff_file
  --let $_rpl_diff_first= 0
}
--remove_file $_rpl_diff_prev_file
--remove_file $_rpl_diff_statement_file

--let $include_filename= rpl_diff.inc
--source include/end_include_file.inc
