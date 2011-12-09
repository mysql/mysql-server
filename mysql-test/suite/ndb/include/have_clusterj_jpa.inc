--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;

#
# Look for Cluster/J JPA library, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);

#
# Check if the needed jars are available
#
my $clusterj_jpa_jar = my_find_file($basedir,
                                ["storage/ndb/clusterj/clusterj-openjpa", 
                                 "share/mysql/java",             # install unix
                                 "lib/java"],                    # install windows
                                "clusterjpa-*.jar", NOT_REQUIRED);

my $clusterj_jpa_test_jar = my_find_file($basedir,
                                    ["storage/ndb/clusterj/clusterj-jpatest", 
                                     "share/mysql/java",             # install unix
                                     "lib/java"],                    # install windows
                                    "clusterj-jpatest-*.jar", NOT_REQUIRED);

my $F = IO::File->new("$vardir/tmp/have_clusterj_jpa_result.inc", 'w') or die;
if ($clusterj_jpa_jar) {
  print $F "--let \$CLUSTERJ_JPA_JAR= $clusterj_jpa_jar\n"; 
  print $F "--echo Found clusterj-openjpa.jar: '\$CLUSTERJ_JPA_JAR'\n"
} else {
  print $F "skip Could not find clusterj jpa jar file\n";
}

if ($clusterj_jpa_test_jar) {
  print $F "--let \$CLUSTERJ_JPA_TEST_JAR= $clusterj_jpa_test_jar\n";
  print $F "--echo Found clusterj_jpa_test jar: '\$CLUSTERJ_JPA_TEST_JAR'\n"
} else {
  print $F "skip Could not find clusterj jpa test jar file\n";
}

$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_clusterj_jpa_result.inc
