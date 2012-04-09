--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for Cluster/J library, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);

#
# Check if the needed jars and libs are available
#

my $clusterj_jar = my_find_file($basedir,
                                ["storage/ndb/clusterj", 
                                 "share/mysql/java",             # install unix
                                 "lib/java"],                    # install windows
                                "clusterj-*.jar", NOT_REQUIRED);

my $clusterj_test_jar = my_find_file($basedir,
                                    ["storage/ndb/clusterj/clusterj-test",
                                     "share/mysql/java",             # install unix
                                     "lib/java"],                    # install windows
                                    "clusterj-test-*.jar", NOT_REQUIRED);

my $ndbclient_lib = my_find_file($basedir,
                                ["storage/ndb/src/.libs", 
                                 "storage/ndb/src", 
                                 "lib/mysql",
                                 "lib"],
                                ["ndbclient.dll", "libndbclient.so", "libndbclient.dylib"],
                                NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_clusterj_result.inc", 'w') or die;
if ($clusterj_jar) {
  print $F "--let \$CLUSTERJ_JAR= $clusterj_jar\n"; 
  print $F "--echo Found clusterj.jar: '\$CLUSTERJ_JAR'\n"
} else {
  print $F "skip Could not find clusterj jar file\n";
}

if ($clusterj_test_jar) {
  print $F "--let \$CLUSTERJ_TEST_JAR= $clusterj_test_jar\n";
  print $F "--echo Found clusterj_test jar: '\$CLUSTERJ_TEST_JAR'\n"
} else {
  print $F "skip Could not find clusterj test jar file\n";
}

if ($ndbclient_lib) {
  my $ndbclient_lib_dir = dirname($ndbclient_lib);
  print $F "--let \$NDBCLIENT_LIB_DIR= $ndbclient_lib_dir\n";
} else {
    print $F "skip Did not find libndbclient\n";
}

$F->close();

write_properties();

sub add {
   my $F = shift;
   my $prop = shift;
   my $val = shift;
   print $F "com.mysql.clusterj.$prop=$val\n";
}

sub write_properties {
  my $db = 'test';
  my $user = 'root';
  my $F = IO::File->new("$vardir/tmp/clusterj.properties", 'w') or die;
  add($F, 'connectstring', $ENV{'NDB_CONNECTSTRING'});
  add($F, 'connect.retries', '4');
  add($F, 'connect.delay', '5');
  add($F, 'connect.verbose', '1');
  add($F, 'connect.timeout.before', '30');
  add($F, 'connect.timeout.after', '20');
  add($F, 'jdbc.url', "jdbc:mysql://localhost:$ENV{MASTER_MYPORT}/$db");
  add($F, 'jdbc.driver', 'com.mysql.jdbc.Driver');
  add($F, 'jdbc.username', $user);
  add($F, 'jdbc.password', '');
  add($F, 'username', '');
  add($F, 'password', '');
  add($F, 'database', $db);
  add($F, 'max.transactions', '1024');
  $F->close();
}

EOF

--source $MYSQLTEST_VARDIR/tmp/have_clusterj_result.inc
