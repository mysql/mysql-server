# Purpose:
# To get the immediate_commit_timestamp value from the
# current connection server's current binlog file for a
# particular GTID.
#
# Parameters:
# --let $gtid=server_uuid:id
#
# $readable
# By default thie file returns the Immediate_commit_timestamp as a
# utc timestamp. If this variable is set this file returns the
# Immediate commit timestamp in human readable form.
#
# $only_time
# If this parameter is set then this file returns only the time
# portion of the human readable immediate_commit_timestamp. Note
# that this parameter has an effect only when $readable is set.
#
# Usage:
# --let $gtid=85e5e835-dcd2-11e3-a2ce-3c970e9d6412:2
# --source include/get_immediate_commit_timestamp.inc
#
# Return Value:
# $immediate_commit_timestamp
#

--let $MYSQLD_DATADIR = `select @@datadir`
--let $binlog_file = query_get_value(SHOW MASTER STATUS, File, 1)
--let $prefix = `select uuid()`
--let $out_file = $MYSQLTEST_VARDIR/tmp/$prefix.out
--exec $MYSQL_BINLOG --include-gtids=$gtid --force-if-open $MYSQLD_DATADIR/$binlog_file > $out_file

--let GTID_VAR = $gtid
--let OUTFILE = $out_file
--let READ_VAR = $readable
--let TIME_VAR = $only_time

perl;
  use strict;
  my $outfile = $ENV{'OUTFILE'} or die "OUTFILE not set";
  my $gtid = $ENV{'GTID_VAR'} or die "GTID_VAR not set";
  my $readable = $ENV{'READ_VAR'};
  my $only_time = $ENV{'TIME_VAR'};

  open(FILE, "$outfile") or die("Unable to open $outfile: $!\n");

  my $gtid_next_stmt;
  my $prev;
  my $prev_prev;

   while (my $gtid_next_stmt = <FILE>)
  {
    chomp $gtid_next_stmt;
    if ($gtid_next_stmt =~ /$gtid/ )
    {
      last;
    }
    $prev_prev=$prev;
    $prev=$gtid_next_stmt;
  }

  if ($readable == 1 )
  {
    if ($only_time == 1)
    {
      $prev_prev =~ s/.*([0-9][0-9]:[0-9][0-9]:[0-9][0-9]*).*/\1/;
    }
    else
    {
     $prev_prev =~ s/.*([0-9]{4}-[0-9]*-[0-9]* [0-9][0-9]:[0-9][0-9]:[0-9][0-9]*).*/\1/;
    }
  }
  else
  {
    $prev_prev =~ s/.*immediate_commit_timestamp=([0-9][0-9]*).*/\1/;
  }
  my $dir = $ENV{'MYSQLTEST_VARDIR'};
  open (OUTPUT, ">$dir/tmp/tar.inc") ;
  print OUTPUT "--let \$immediate_commit_timestamp =  $prev_prev\n";
  close (OUTPUT);

EOF

--source $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $out_file
