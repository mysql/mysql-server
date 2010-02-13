--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for ndbjtie library, if not found: skip test.
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
#my $ver = $ENV{MYSQL_SERVER_VERSION};
my $ndbjtie_jar = my_find_file($basedir,
                            ["storage/ndb/src/ndbjtie", "share/mysql/java"],
                            "ndbjtie.jar", NOT_REQUIRED);

my $ndbclient_lib = my_find_file($basedir,
                                ["storage/ndb/src/.libs", "lib/mysql"],
                                ["libndbclient.dll", "libndbclient.so", "libndbclient.dylib"],
                                NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_ndbjtie_result.inc", "w") or die;
if ($ndbjtie_jar) {
  my $ndbjtie_dir = dirname(dirname($ndbjtie_jar));
  print $F "--let \$NDBJTIE_JAR= $ndbjtie_jar\n"; 
  print $F "--let \$NDBJTIE_DIR= $ndbjtie_dir\n";
  print $F "--echo Found ndbjtie jar: '\$NDBJTIE_JAR'\n";
} else {
  print $F "skip Could not find ndbjtie jar file\n";
}

if ($ndbclient_lib) {
  my $ndbclient_lib_dir = dirname($ndbclient_lib);
  print $F "--let \$NDBCLIENT_LIB_DIR= $ndbclient_lib_dir\n";
} else {
    print $F "skip Did not find libndbclient\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_ndbjtie_result.inc
