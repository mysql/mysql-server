--source include/not_embedded.inc
--source include/not_windows.inc

--perl

use strict;
use File::Basename;
use IO::File;
use lib "lib/";
use My::Platform;
use My::Find;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $basedir = dirname($mysql_test_dir);

my $found_perl_source = my_find_file($basedir,
   ["storage/ndb/memcache",        # source
    "mysql-test/lib"],             # install
    "memcached_path.pl", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_memcache_result.inc", "w") or die;

if ($found_perl_source eq '') {
  print $F "--skip Could not find NDB Memcache API\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_memcache_result.inc


