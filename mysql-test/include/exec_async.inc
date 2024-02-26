# ==== Purpose ====
#
# Execute a command asynchronously, similar to using a command line
# that ends with '&' in the shell.
#
# ==== Usage ====
#
# --let $exec_command = COMMAND
# [--let $rpl_debug = 1]
# --source include/exec_async.inc
#
# Parameters:
#
# $exec_command
#   The command to execute. This should not contain the trailing
#   '> /dev/null' or '&' elements; they will be added by this script in
#   a portable way.
#
# $rpl_debug
#   If enabled, prints the command line.

--echo # Executing asynchronous command
--let $dev_null = /dev/null
--source include/check_windows.inc
if ($have_windows) {
  --let $dev_null = NUL
}
--let EXEC_COMMAND_ENV = $exec_command > $dev_null &
if ($rpl_debug) {
  --echo # debug: executing $EXEC_COMMAND_ENV
}
perl;
  my $command_line = $ENV{'EXEC_COMMAND_ENV'};
  system($command_line);
EOF
