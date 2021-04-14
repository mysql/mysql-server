# ==== Purpose ====
#
# Start the server given by $rpl_server_number.  This should normally
# be invoked after rpl_stop_server.inc.
#
# ==== Usage ====
#
# --let $rpl_server_number= N
# [--let $rpl_start_with_gtids= [0|1]]
# [--let $rpl_server_parameters= --flag1 --flag2 ...]
# [--let $rpl_omit_print_server_parameters= 0]
# [--let $rpl_debug_trace= 1]
# [--let $rpl_debug= 1]
# [--let $rpl_server_error= 0]
# --source include/rpl_start_server.inc
#
# Parameters:
#
#   $rpl_server_number
#     Number to identify the server that needs to reconnect.  1 is the
#     master server, 2 the slave server, 3 the 3rd server, and so on.
#     Cf. include/rpl_init.inc
#
#   $rpl_start_with_gtids
#     If set to 1, the server will start with GTIDs on, i.e.:
#       --gtid-mode=on --enforce-gtid-consistency
#       --log-bin --log-replica-updates
#       --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB
#     If set to 0, the server will start without GTIDs, i.e.:
#       --gtid-mode=off
#     If not set, the server will start with default parameters (depends on
#     my.cnf and .opt file).
#
#   $rpl_server_parameters
#     If set, extra parameters given by this variable are passed to
#     mysqld.
#
#   $rpl_omit_print_server_parameters
#     If set, server parameters are not printed.
#
#   $rpl_debug_trace
#     Start the server with debug traces enabled, as with mtr --debug.
#     Normally, you can just pass --debug to mtr. However, mtr --debug
#     will cancel the effect of any --debug parameters specified in
#     $rpl_server_parameters.  If $rpl_server_parameters contains
#     --debug, set $rpl_debug_trace=1 instead of passing --debug to
#     mtr.
#
#   $rpl_debug
#     See include/rpl_init.inc
#
#   $rpl_server_error
#     If set, failure of the server startup is expected. 
#
# ==== See also ====
#
# rpl_stop_server.inc
# rpl_restart_server.inc

# Pretty-printed text
--let $_rpl_start_server_info= server_number=$rpl_server_number
# Actual arguments sent to mysqld
--let _RPL_START_SERVER_PARAMETERS= $rpl_server_parameters

if ($rpl_start_with_gtids == 1)
{
  --let $_rpl_start_server_info= $_rpl_start_server_info gtids=on
  --let _RPL_START_SERVER_PARAMETERS= $_RPL_START_SERVER_PARAMETERS --log-bin --log-replica-updates --enforce-gtid-consistency --gtid-mode=on --default-storage-engine=InnoDB --default-tmp-storage-engine=InnoDB
}
if ($rpl_start_with_gtids == 0)
{
  --let $_rpl_start_server_info= $_rpl_start_server_info gtids=off
  --let _RPL_START_SERVER_PARAMETERS= $_RPL_START_SERVER_PARAMETERS --gtid-mode=off
}
if ($rpl_server_parameters != '')
{
  if (!$rpl_omit_print_server_parameters)
  {
    --let $_rpl_start_server_info= $_rpl_start_server_info parameters: $rpl_server_parameters
  }
}
if ($rpl_debug)
{
  --echo # debug: rpl_server_number='$rpl_server_number' rpl_start_with_gtids='$rpl_start_with_gtids' rpl_server_parameters='$rpl_server_parameters' _RPL_START_SERVER_PARAMETERS='$_RPL_START_SERVER_PARAMETERS'
}

--let $include_filename= rpl_start_server.inc [$_rpl_start_server_info]
--source include/begin_include_file.inc

--let $rpl_connection_name= server_$rpl_server_number
--source include/rpl_connection.inc

--let _RPL_DEBUG_TRACE= $rpl_debug_trace
--let _RPL_SERVER_NUMBER= $rpl_server_number
--let _RPL_DEBUG= $rpl_debug

# Write file to make mysql-test-run.pl start up the server again
# Because mysqltest is such a wonderful language, we use perl instead.
perl;
  my $vardir= $ENV{'MYSQLTEST_VARDIR'};
  my $params= $ENV{'_RPL_START_SERVER_PARAMETERS'};
  my $server_number= $ENV{'_RPL_SERVER_NUMBER'};
  my $trace_file= "$vardir/log/mysqld.$server_number.trace";
  my $expect_file= "$vardir/tmp/mysqld.$server_number.expect";
  if ($ENV{'_RPL_DEBUG_TRACE'})
  {
    my $flags="d:i:a$trace_file:t";
    ($params =~ s/(?<=--debug=)/$flags:/) or ($params .= " --debug=$flags");
  }
  if ($ENV{'_RPL_DEBUG'})
   {
    print "# debug: params='$params'\n";
  }
  # Write trace file
  open FILE, ">> $trace_file" or die "Error opening trace file '$trace_file': $!";
  print FILE "\n======== SERVER START \[" . scalar(localtime()) . "\] ========\n"
    or die "Error printing to trace file '$trace_file': $!";
  close FILE or die "Error closing trace file '$trace_file': $!";
  # Write expect file
  open FILE, "> $expect_file" or die "Error opening $expect_file: $!";
  print FILE ($params eq '' ? 'restart' : "restart:$params")
    or die "Error printing to $expect_file: $!";
  close FILE or die "Error closing $expect_file: $!";
EOF

#--exec echo "$_rpl_start_server_command" > $MYSQLTEST_VARDIR/tmp/mysqld.$rpl_server_number.expect

if (!$rpl_server_error)
{
  --source include/rpl_reconnect.inc

  --let $include_filename= rpl_start_server.inc [$_rpl_start_server_info]
  --source include/end_include_file.inc
}

if ($rpl_server_error)
{
  --dec $_include_file_depth
}
