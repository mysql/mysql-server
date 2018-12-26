# Check that the latest checkpoint in the redo log files
# is not newer than the checkpoint sampled by no_checkpoint_start.inc

--source include/kill_mysqld.inc

perl;
my $cp = $ENV{CHECKPOINT_LSN};
$cp =~ s/^InnoDB\t\t//;
my $log = "$ENV{MYSQLD_DATADIR}ib_logfile0";
open(LOG, "<$log") || die "Unable to open $log";
seek(LOG, 512, 0) || die "Unable to seek $log";
die unless read(LOG, $_, 16) == 16;
my ($no1hi,$no1lo,$cp1hi,$cp1lo) = unpack("N*", $_);
seek(LOG, 3 * 512, 0) || die "Unable to seek $log";
die unless read(LOG, $_, 16) == 16;
my ($no2hi,$no2lo,$cp2hi,$cp2lo) = unpack("N*", $_);
close(LOG);

my $cp1 = $cp1hi << 32 | $cp1lo;
my $cp2 = $cp2hi << 32 | $cp2lo;

open(OUT, ">$ENV{MYSQLTEST_VARDIR}/log/check.txt") || die;

if ($cp1 > $cp || $cp2 > $cp) {
   print OUT "--source include/start_mysqld.inc\n";
   print OUT "$ENV{CLEANUP_IF_CHECKPOINT}\n";
   print OUT "$ENV{CLEANUP_FILES_IF_CHECKPOINT}\n";
   print OUT "--skip Extra checkpoint 1 after $cp";
   print OUT " ($no1hi:$no1lo=$cp1,$no2hi:$no2lo=$cp2)\n";
}

close(OUT);
EOF

--source $MYSQLTEST_VARDIR/log/check.txt
--remove_file $MYSQLTEST_VARDIR/log/check.txt
