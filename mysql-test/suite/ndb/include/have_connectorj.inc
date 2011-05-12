--perl
use strict;
use IO::File;

#
# Looks for MySQL Connector/J JAR file in MTR_CLASSPATH
#

my $ok = 0;
my $classpath = $ENV{'MTR_CLASSPATH'} || '';
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $jar;
my $notfound;

# -- look for the Connctor/J jar file
foreach $jar (split ':', $classpath) {
  if($jar =~ m/mysql-connector/  && -f $jar) {
    $ok = 2;
  }
  elsif($jar =~ m/mysql-connector/) {
    $ok = 1;
    $notfound = $jar;
  }
}

# -- write the result to the output file
my $F = IO::File->new("$vardir/tmp/have_connectorj_result.inc", "w") or die;
if($ok == 2) {
  print $F  "--echo Found Connector/J JAR in MTR_CLASSPATH\n";
}
elsif($ok == 1) {
  print $F  "--skip JAR file not found: \"$notfound\"\n";
} 
else {
  print $F  "--skip Please specify Connector/J JAR file in MTR_CLASSPATH env variable\n";
}

$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_connectorj_result.inc
