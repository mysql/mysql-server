--perl
use strict;

use IO::File;

#
# Look for Apache OpenJPA library, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";

my $classpath = $ENV{'MTR_CLASSPATH'} || '';

my $F = IO::File->new("$vardir/tmp/have_openjpa_result.inc", 'w') or die;
if ($classpath =~ m/openjpa/g) {
  print $F "--echo Found clusterj-openjpa.jar in MTR_CLASSPATH\n"
} else {
  print $F "skip Could not find Apache OpenJPA jar in MTR_CLASSPATH ($classpath)\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_openjpa_result.inc
