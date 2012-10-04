--source include/not_windows.inc

--perl
my $MYSQL = $ENV{'MYSQL'};
my $MYSQLADMIN = $ENV{'MYSQLADMIN'};
my $MASTER_MYPORT = $ENV{'MASTER_MYPORT'};
my $MYSQLTEST_VARDIR = $ENV{'MYSQLTEST_VARDIR'};
my $LOG_FILE = $ENV{'WL6301_LOG_FILE'};

my @ipv4_addresses = split (/\n/, `ifconfig | grep 'inet addr' | sed 's/^.*inet addr://' | sed 's/ .*\$//'`);
my @ipv6_addresses = split (/\n/, `ifconfig | grep 'inet6 addr.*Scope:Global' | sed 's/^.*inet6 addr: //' | sed 's/ .*\$//' | sed 's|/.*\$||'`);

push (@ipv6_addresses, '::1');

open (LOGFH, ">$LOG_FILE") or
  die "Can not open '$LOG_FILE': $!\n";

my $ipv4_failed = 0;

foreach my $ip (@ipv4_addresses)
{
  print LOGFH "- ipv4: '$ip'\n";

  my $rc =
    system(
      "$MYSQL " .
      "--host=127.0.0.1 " .
      "--port=$MASTER_MYPORT " .
      "--user=root " .
      "test " .
      "-e 'GRANT ALL PRIVILEGES ON test.* TO u1@$ip;'");

  print LOGFH "  GRANT status: $rc\n";

  $ipv4_failed |= $rc;

  $rc =
    system(
      "$MYSQLADMIN " .
      "--host=$ip " .
      "--port=$MASTER_MYPORT " .
      "--user=u1 " .
      "ping > /dev/null 2>&1");

  print LOGFH "  Connect u1@$ip status: $rc\n";

  $ipv4_failed |= $rc;

  $rc =
    system(
      "$MYSQL " .
      "--host=127.0.0.1 " .
      "--port=$MASTER_MYPORT " .
      "--user=root " .
      "test " .
      "-e 'DROP USER u1@$ip;'");

  print LOGFH "  DROP USER status: $rc\n";

  $ipv4_failed |= $rc;
}

my $ipv6_failed = 0;

foreach my $ip (@ipv6_addresses)
{
  print LOGFH "- ipv6: '$ip'\n";

  my $rc =
    system(
      "$MYSQL " .
      "--host=127.0.0.1 " .
      "--port=$MASTER_MYPORT " .
      "--user=root " .
      "test " .
      "-e 'GRANT ALL PRIVILEGES ON test.* TO u1@$ip;'");

  print LOGFH "  GRANT status: $rc\n";

  $ipv6_failed |= $rc;

  $rc =
    system(
      "$MYSQLADMIN " .
      "--host=$ip " .
      "--port=$MASTER_MYPORT " .
      "--user=u1 " .
      "ping > /dev/null 2>&1");

  print LOGFH "  Connect u1@$ip status: $rc\n";

  $ipv6_failed |= $rc;

  $rc =
    system(
      "$MYSQL " .
      "--host=127.0.0.1 " .
      "--port=$MASTER_MYPORT " .
      "--user=root " .
      "test " .
      "-e 'DROP USER u1@$ip;'");

  print LOGFH "  DROP USER status: $rc\n";

  $ipv6_failed |= $rc;
}

close LOGFH;

unless ($ipv4_failed)
{
  print "IPv4 connectivity: OK\n";
}
else
{
  print "IPv4 connectivity: FAIL\n";
}

unless ($ipv6_failed)
{
  print "IPv6 connectivity: OK\n";
}
else
{
  print "IPv6 connectivity: FAIL\n";
}

EOF
