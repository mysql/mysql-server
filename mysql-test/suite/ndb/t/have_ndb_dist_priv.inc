--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for ndb_dist_pric.sql, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);

#
# Check if the needed tests are available
#
my $sql_file = my_find_file($basedir,
                            ["storage/ndb/tools", "share/mysql/"],
                            "ndb_dist_priv.sql", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndb_dist_priv_result.inc", "w") or die;
if ($sql_file) {
  print $F "--let \$NDB_DIST_PRIV_SQL= $sql_file\n";
}
else
{
  print $F "skip Could not find ndb_dist_priv.sql;\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndb_dist_priv_result.inc
