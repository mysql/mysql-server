# Purpose:
# To get the immediate_server_version value from the current connection server's
# current binlog file or from a provided binlog or relay log for a particular
# GTID.
#
# Parameters:
# --let $gtid=server_uuid:id
# [--let $log_file = /path/to/log/file ]
#
# Usage:
# --let $gtid=85e5e835-dcd2-11e3-a2ce-3c970e9d6412:2
# [--let $log_file = $server_2_datadir/server-relay-log.000002]
# --source include/rpl/get_immediate_server_version.inc
#
# Return Value:
# $immediate_server_version
#

if ($log_file == '' )
{
  --let $MYSQLD_DATADIR = `select @@datadir`
  --let $binlog_file = query_get_value(SHOW BINARY LOG STATUS, File, 1)
  --let $log_file = $MYSQLD_DATADIR/$binlog_file
}
--let $prefix = `select uuid()`
--let $out_file = $MYSQLTEST_VARDIR/tmp/$prefix.out
--exec $MYSQL_BINLOG --include-gtids=$gtid --force-if-open $log_file > $out_file

--let GTID_VAR = $gtid
--let OUTFILE = $out_file

perl;
  use strict;
  my $outfile = $ENV{'OUTFILE'} or die "OUTFILE not set";
  my $gtid = $ENV{'GTID_VAR'} or die "GTID_VAR not set";
  open(FILE, "$outfile") or die("Unable to open $outfile: $!\n");

  my $gtid_next_stmt;
  my $ISV;

  while (my $gtid_next_stmt = <FILE>)
  {
    chomp $gtid_next_stmt;
    if ($gtid_next_stmt =~ /$gtid/ )
    {
      last;
    }
    $ISV=$gtid_next_stmt;
  }

  $ISV =~ s/.*immediate_server_version=([0-9][0-9]*).*/\1/;
  my $dir = $ENV{'MYSQLTEST_VARDIR'};
  open (OUTPUT, ">$dir/tmp/tar.inc") ;
  print OUTPUT "--let \$immediate_server_version = $ISV\n";
  close (OUTPUT);

EOF

--let $log_file =
--source $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $MYSQLTEST_VARDIR/tmp/tar.inc
--remove_file $out_file
