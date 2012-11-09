--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for
# - RQG_HOME
# - binaries needed to run ndb rqg scripts
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

#
# Check if the needed jars and libs are available
#

my $runall = my_find_file($basedir,
                          ["storage/ndb/test/rqg" ],
                          "load_and_run.sh", NOT_REQUIRED);

my $loadrqg = my_find_file($basedir,
                          ["storage/ndb/test/rqg" ],
                          "load_rqg.sh", NOT_REQUIRED);

my $runrqg = my_find_file($basedir,
                          ["storage/ndb/test/rqg" ],
                          "run_rqg.sh", NOT_REQUIRED);

my $exe_mysqltest = my_find_bin($bindir,
		                ["client", "bin"],
                                "mysqltest", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndb_rqg.inc", 'w') or die;
if ($ENV{RQG_HOME}) {
  print $F "--echo RQG_HOME: $ENV{RQG_HOME}\n"
} else {
  print $F "skip Could not find RQG, set environment variable RQG_HOME\n";
}

my $env_mysqltest="";
if ($exe_mysqltest) {
  $env_mysqltest="EXE_MYSQLTEST=$exe_mysqltest"
}
else {
  print $F "skip Could find mysqltest binary\n";
}


if ($runall) {
  print $F "--let \$RUN_ALL= $env_mysqltest bash $runall -p \$MASTER_MYPORT\n"
} else {
  print $F "skip Could not find load_and_run.sh\n";
}

if ($loadrqg) {
  print $F "--let \$LOAD_RQG= $env_mysqltest bash $loadrqg -p \$MASTER_MYPORT\n"
} else {
  print $F "skip Could not find load_rqg.sh\n";
}

if ($runrqg) {
  print $F "--let \$RUN_RQG= $env_mysqltest bash $runrqg -p \$MASTER_MYPORT\n"
} else {
  print $F "skip Could not find run_rqg.sh\n";
}

$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndb_rqg.inc
