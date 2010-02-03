--perl
use strict;
use IO::File;

#
# Looks for JUnit JAR file in MTR_CLASSPATH
#

my $classpath = $ENV{'MTR_CLASSPATH'} || '';

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_junit_result.inc", "w") or die;
if ($classpath =~ m/junit/) {
  print $F "--echo Found JUnit JAR in MTR_CLASSPATH\n";
} else {
  print $F "--skip Could not find JUnit JAR file, please set MTR_CLASSPATH env variable\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_junit_result.inc
