--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for ndbjtie JUnit tests, if not found: skip test.
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
my $jtie_junit = my_find_file($bindir,
                            ["storage/ndb/src/ndbjtie/test", "share/mysql/java"],
                            "ndbjtie-test-*.jar", NOT_REQUIRED);

my $jtie_unload_junit = my_find_file($bindir,
                            ["storage/ndb/src/ndbjtie/jtie/test/unload", "share/mysql/java"],
                            "jtie-test-unload-*.jar", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndbjtie_junit_result.inc", "w") or die;
if ($jtie_junit) {
  my $jtie_junit_dir = dirname($jtie_junit);
  print $F "--let \$NDBJTIE_JUNIT= $jtie_junit_dir\n";
  print $F "--let \$NDBJTIE_JUNIT_JAR= $jtie_junit\n";
  print $F "--let \$NDBJTIE_UNLOAD_JUNIT_JAR= $jtie_unload_junit\n";
  print $F "--echo Found NDB JTie JUnit tests: '\$NDBJTIE_JUNIT_JAR'\n"
} else {
  print $F "skip Could not find NDB JTie JUnit tests\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndbjtie_junit_result.inc
