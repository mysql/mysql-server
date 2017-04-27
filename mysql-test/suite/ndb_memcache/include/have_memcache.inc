--source include/not_windows.inc

--perl

use strict;
use File::Basename;
use IO::File;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $memcached_started = $ENV{NDB_MEMCACHED_STARTED};

my $F = IO::File->new("$vardir/tmp/have_memcache_result.inc", "w") or die;
if (!$memcached_started) {
  print $F "--skip Could not find NDB Memcache API\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_memcache_result.inc


