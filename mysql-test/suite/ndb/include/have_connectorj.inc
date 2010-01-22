--perl
use strict;
use IO::File;

#
# Looks for MySQL Connector/J JAR file
# - MTR_CONNECTOR_J environment variable
#
# If Connector/J JAR file is found, CONNECTORJ_JAR will be set to reflect this location
#

my $connectorj_jar = $ENV{'MTR_CONNECTORJ_JAR'};

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/have_connectorj_result.inc", "w") or die;
if ($connectorj_jar eq '') {
  print $F "--skip Could not find Connector/J JAR file, please set MTR_CONNECTORJ_JAR env variable\n";
} else {
  print $F "--let \$CONNECTORJ_JAR= $connectorj_jar\n";
  print $F "--echo Found Connector/J JAR: '\$CONNECTORJ_JAR'\n";
}
$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_connectorj_result.inc
