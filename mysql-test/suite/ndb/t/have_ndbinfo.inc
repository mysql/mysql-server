--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for ndbinfo.sql, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

#
# Check if the needed tests are available
#
my $sql_file = my_find_file($bindir,
                            ["storage/ndb/tools", "share/mysql/"],
                            "ndbinfo.sql", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndbinfo_result.inc", "w") or die;
if ($sql_file) {
  print $F "--let \$NDBINFO_SQL= $sql_file\n";
}
else
{
  print $F "skip Could not find ndbinfo.sql;\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndbinfo_result.inc
