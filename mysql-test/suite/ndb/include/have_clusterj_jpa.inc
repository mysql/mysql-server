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
                                ["storage/ndb/clusterj/clusterj-openjpa", "share/mysql/java"],
                                "clusterjpa-*.jar", NOT_REQUIRED);

my $clusterj_jpa_test_jar = my_find_file($basedir,
                                    ["storage/ndb/clusterj/clusterj-jpatest", "share/mysql/java"],
                                    "clusterj-jpatest-*.jar", NOT_REQUIRED);

my $clusterj_test_schema = my_find_file($basedir,
                                    ["storage/ndb/clusterj/clusterj-test/src/main/resources", "share/mysql/java"],
                                    "schema.sql", NOT_REQUIRED);

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

if ($clusterj_test_schema) {
  print $F "--let \$CLUSTERJ_TEST_SCHEMA= $clusterj_test_schema\n";
  print $F "--echo Found clusterj_test schema: '\$CLUSTERJ_TEST_SCHEMA'\n"
} else {
  print $F "skip Could not find clusterj test schema file\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_clusterj_jpa_result.inc
