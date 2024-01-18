# Purpose:
# To get the original_commit_timestamp value from the
# current connection server's current binlog file for a
# particular GTID.
#
# Parameters:
# --let $gtid=server_uuid:id
#
# Usage:
# --let $gtid=85e5e835-dcd2-11e3-a2ce-3c970e9d6412:2
# --source include/rpl/get_original_commit_timestamp.inc
#
# Return Value:
# $original_commit_timestamp
#

--let $MYSQLD_DATADIR = `select @@datadir`
--let $binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $prefix = `select uuid()`
--let $out_file = $MYSQLTEST_VARDIR/tmp/$prefix.out
--exec $MYSQL_BINLOG --include-gtids=$gtid --force-if-open $MYSQLD_DATADIR/$binlog_file > $out_file

--let GTID_VAR = $gtid
--let OUTFILE = $out_file

perl;
  use strict;
  my $outfile = $ENV{'OUTFILE'} or die "OUTFILE not set";
  my $gtid = $ENV{'GTID_VAR'} or die "GTID_VAR not set";
  open(FILE, "$outfile") or die("Unable to open $outfile: $!\n");

  my $gtid_next_stmt;
  my $prev_gtid_next_stmt;
  my $prev_prev;
  my $OCT;

  while (my $gtid_next_stmt = <FILE>)
  {
    chomp $gtid_next_stmt;
    if ($gtid_next_stmt =~ /$gtid/ )
    {
      last;
    }
    $OCT=$prev_prev;
    $prev_prev=$prev_gtid_next_stmt;
    $prev_gtid_next_stmt=$gtid_next_stmt;
  }

  $OCT =~ s/.*original_commit_timestamp=([0-9][0-9]*).*/\1/;
  my $dir = $ENV{'MYSQLTEST_VARDIR'};
  open (OUTPUT, ">$dir/tmp/tar.inc") ;
  print OUTPUT "--let \$original_commit_timestamp =  $OCT\n";
  close (OUTPUT);

EOF

--source $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $out_file
