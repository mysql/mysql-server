# Purpose:
# To get the time of particular gtid event
#
# Parameters:
# --let $gtid=server_uuid:id
#
# Usage:
# --let $gtid=85e5e835-dcd2-11e3-a2ce-3c970e9d6412:2
# --source include/get_gtid_timestamp.inc
#
# Return Value:
# $gtid_time
# Timestamp of gtid event in the binary log.

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
  my $prev1;
  my $prev2;
  my $prev3;
  my $prev4;
  my $match;

  while (my $gtid_next_stmt = <FILE>)
  {
    chomp $gtid_next_stmt;
    if ($gtid_next_stmt =~ /$gtid/ )
    {
      last;
    }
    if ($gtid_next_stmt =~ /ISOLATION/ )
    {
      next;
    }
    $prev4=$prev3;
    $prev3=$prev2;
    $prev2=$prev1;
    $prev1=$gtid_next_stmt;
  }
  $match=0;
  if ($prev4 =~ m/.*[0-9][0-9]:[0-9][0-9]:[0-9][0-9].*/)
  {
    $prev4 =~ s/.*([0-9][0-9]:[0-9][0-9]:[0-9][0-9]).*/\1/;
    $match=1;
  }

  if (!$match)
  {
    $prev4 =~ s/.*([0-9]:[0-9][0-9]:[0-9][0-9]).*/\1/;
  }

  my $dir = $ENV{'MYSQLTEST_VARDIR'};
  open (OUTPUT, ">$dir/tmp/tar.inc") ;
  print OUTPUT "--let \$gtid_time =  $prev4\n";
  close (OUTPUT);

EOF

--source $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $out_file
