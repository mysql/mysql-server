--perl
use strict;
use IO::File;

#
# Looks for JUnit JAR file
# - MTR_JUNIT_JAR environment variable
#
# If jar file is found, JUNIT_JAR will be set to reflect this location.
#

my $junit_jar = $ENV{'MTR_JUNIT_JAR'};

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_junit_result.inc", "w") or die;
if ($junit_jar eq '') {
  print $F "--skip Could not find JUnit JAR file, please set MTR_JUNIT_JAR env variable\n";
} else {
  print $F "--let \$JUNIT_JAR= $junit_jar\n";
  print $F "--echo Found JUnit JAR: '\$JUNIT_JAR'\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_junit_result.inc
