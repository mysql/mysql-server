# ==== Purpose ====
#
# Shut down mysqlrouter.
#
# ==== Usage ====
#
# --let $pid_file = /path/to/the/file.pid
# --source shutdown_mysqlrouter.inc
#
if (!$pid_file)
{
  die "The shutdown script, requires that PID file is specified.";
}
--let MYSQLROUTER_PIDFILE = $pid_file

perl;
open(my $FILE, "+<", $ENV{'MYSQLROUTER_PIDFILE'}) or die "Can't open mysqlrouters PID file.";
my $pid= (<$FILE> + 0) or die "MySQLRouter PID file contains invalid process ID.";

if ( $^O eq 'MSWin32')
{
  eval "use Win32::Process";
  Win32::Process::KillProcess($pid, 201);
}
else {
  kill 'TERM',$pid;
}

close($FILE);
EOF
