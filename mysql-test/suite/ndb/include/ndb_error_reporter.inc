--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
# Out of source set MTR_BINDIR
my $bindir = $ENV{MTR_BINDIR} || $basedir;
my $ndb_error_reporter = my_find_file($bindir,
                            ["storage/ndb/tools", "bin"],
                            "ndb_error_reporter");
my $F = IO::File->new("$vardir/tmp/ndb_error_reporter_result.inc", "w") or die;
if ($ndb_error_reporter) {
  print $F "--let \$NDB_ERROR_REPORTER=$ndb_error_reporter\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/ndb_error_reporter_result.inc
