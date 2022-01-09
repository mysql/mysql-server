# Check that the latest checkpoint in the redo log files
# is not newer than the checkpoint sampled by no_checkpoint_start.inc

--source include/kill_mysqld.inc

perl;
require 'include/ib_redo_log_files.inc';

my $cp = $ENV{CHECKPOINT_LSN};
$cp =~ s/^InnoDB\t\t//;

open(OUT, ">$ENV{MYSQLTEST_VARDIR}/log/no_checkpoint_cleanup.inc") || die;

foreach my $log_file (log_get_non_tmp_files()) {
    open(LOG, "<$log_file") || die "Unable to open $log_file";
    seek(LOG, 512, 0) || die "Unable to seek $log_file";
    die unless read(LOG, $_, 16) == 16;
    my ($no1hi,$no1lo,$cp1hi,$cp1lo) = unpack("N*", $_);
    seek(LOG, 3 * 512, 0) || die "Unable to seek $log_file";
    die unless read(LOG, $_, 16) == 16;
    my ($no2hi,$no2lo,$cp2hi,$cp2lo) = unpack("N*", $_);
    close(LOG);

    my $cp1 = $cp1hi << 32 | $cp1lo;
    my $cp2 = $cp2hi << 32 | $cp2lo;

    if ($cp1 > $cp || $cp2 > $cp) {
       print OUT "--source include/start_mysqld.inc\n";
       print OUT "$ENV{CLEANUP_IF_CHECKPOINT}\n";
       print OUT "--echo \"Unexpected checkpoint 1 after $cp";
       print OUT " ($no1hi:$no1lo=$cp1,$no2hi:$no2lo=$cp2)\"\n";
       print OUT "--exit\n";
       last;
    }
}

close(OUT);
EOF

--source $MYSQLTEST_VARDIR/log/no_checkpoint_cleanup.inc
--remove_file $MYSQLTEST_VARDIR/log/no_checkpoint_cleanup.inc
