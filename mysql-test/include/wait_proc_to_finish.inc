# ==== Purpose ====
#
# Wait for process to finish.
#
# This script can be used as standalone, but most probably
# will be used together with start_proc_in_background.inc
#
# ==== Usage ====
#
# --let $pid_file = pid_file
# --source include/wait_proc_to_finish.inc
#
# Parameters:
#   $pid_file
#     File containing PID of the process.
#

if (!$pid_file)
{
  --die ERROR IN TEST: 'pid_file' parameter specified
}

--let _PID_FILE = $pid_file

perl;
  use Errno;
  use strict;
  my $pid_file = $ENV{'_PID_FILE'};
  open(my $f, '<', $pid_file) or die "OPENING $pid_file: $!\n";
  my $pid = do { local($/); <$f> };
  close($f);
  my $not_present = (!kill(0,$pid) && $! == Errno::ESRCH);
  while (!$not_present) {
    sleep(1);
    $not_present = (!kill(0,$pid) && $! == Errno::ESRCH);
  }
EOF

