# ==== Purpose ====
#
# Check that two or more servers have identical databases; fail if not.
#
# ==== Usage ====
#
# --let $diff_servers= 1 2
# [--let $databases= db1 db2 ...]
# [--let $rpl_debug= 1]
# --source include/diff_servers.inc

# pretty-print header
--let $_ds_info= servers=$diff_servers
if ($databases != '')
{
  --let $_ds_info= $_ds_info databases=$databases
}
--let $include_filename= diff_servers.inc [$_ds_info]
--source include/begin_include_file.inc

# get databases
--let $_ds_databases= $diff_database
if ($_ds_databases == '')
{
  --let $_ds_databases= `SELECT GROUP_CONCAT(SCHEMA_NAME SEPARATOR ' ') FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME NOT IN ('information_schema', 'mtr', 'mysql', 'performance_schema')`
}

# generate command line
--let $_ds_arg= --defaults-group-suffix=.
--let $_ds_number_pos= `SELECT LOCATE('$_ds_arg', '$MYSQL_DUMP') + LENGTH('$_ds_arg')`
--let $_ds_pre_command= `SELECT SUBSTR('$MYSQL_DUMP', 1, $_ds_number_pos - 1)`
--let $_ds_post_command= `SELECT SUBSTR('$MYSQL_DUMP', $_ds_number_pos + 1)`
--let $_ds_post_command= $_ds_post_command --compact --order-by-primary --skip-extended-insert --no-create-info --databases $_ds_databases
--let $_ds_prev_outfile=

# iterate over servers
--let $_ds_servers= $diff_servers
while ($_ds_servers != '')
{
  --let $_ds_server_number= `SELECT SUBSTRING_INDEX('$_ds_servers', ' ', 1)`
  --let $_ds_servers= `SELECT TRIM(SUBSTRING('$_ds_servers', 1 + LENGTH('_$ds_server_number')))`
  --let $_ds_outfile= $MYSQLTEST_VARDIR/tmp/diff_servers_$_ds_server_number

  if ($rpl_debug)
  {
    --echo generating dump from server '$_ds_server_number' using command '$_ds_pre_command$_ds_server_number $_ds_post_command > $_ds_outfile'
    --echo remaining servers: '$_ds_servers'
  }
  --exec $_ds_pre_command$_ds_server_number $_ds_post_command > $_ds_outfile

  if ($_ds_prev_outfile != '')
  {
    if ($rpl_debug)
    {
      --echo diffing files '$_ds_prev_outfile' and '$_ds_outfile'
    }
    --diff_files $_ds_prev_outfile $_ds_outfile

    --remove_file $_ds_prev_outfile
  }
  --let $_ds_prev_outfile= $_ds_outfile
}

--remove_file $_ds_prev_outfile

--let $include_filename= diff_servers.inc [servers=$_ds_info]
--source include/end_include_file.inc
