--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for the tools to create and drop fk, if not found: skip test.
#

#
# Setup paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir;

#
# Check if the necessary tools are available
#
my $create_fk = my_find_bin($bindir,
                             ["storage/ndb/test/tools", "bin"],
                            "create_fk", NOT_REQUIRED);
my $drop_fk = my_find_bin($bindir,
                             ["storage/ndb/test/tools", "bin"],
                            "drop_fk", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndb_fk_result.inc", "w") or die;
if ($create_fk) {
  print $F "--let \$CREATE_FK= $create_fk\n";
}
else
{
  print $F "skip Could not find create_fk;\n";
}
if ($drop_fk) {
  print $F "--let \$DROP_FK= $drop_fk\n";
}
else
{
  print $F "skip Could not find drop_fk;\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndb_fk_result.inc
