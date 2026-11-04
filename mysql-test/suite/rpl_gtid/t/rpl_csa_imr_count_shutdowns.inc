# ==== Purpose ====
#
# Count the server's "Shutdown complete" lines (MY-010910) in an error log,
# i.e. the clean shutdowns it has logged. A server that mysqltest kills after
# $shutdown_server_timeout logs none.
#
# ==== Usage ====
#
# --let SHUTDOWN_COUNT_LOG= <error log file>
# --source suite/rpl_gtid/t/rpl_csa_imr_count_shutdowns.inc
# # $shutdown_count now holds the number of lines.

--let SHUTDOWN_COUNT_INC= $MYSQLTEST_VARDIR/tmp/rpl_csa_imr_count_shutdowns.tmp
--perl
  use strict;
  my $log = $ENV{'SHUTDOWN_COUNT_LOG'} or die "SHUTDOWN_COUNT_LOG not set";
  my $out = $ENV{'SHUTDOWN_COUNT_INC'};
  my $n = 0;
  open(my $fh, '<', $log) or die "Cannot open $log: $!";
  while (<$fh>) { $n++ if /\[MY-010910\] .*Shutdown complete/; }
  close($fh);
  open(my $oh, '>', $out) or die "Cannot write $out: $!";
  print $oh "--let \$shutdown_count= $n\n";
  close($oh);
EOF
--source $SHUTDOWN_COUNT_INC
--remove_file $SHUTDOWN_COUNT_INC
