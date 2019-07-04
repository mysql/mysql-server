--perl
use strict;
use IO::File;
use lib "lib/";
use My::Platform;


#
# Looks for MySQL Connector/J JAR file in MTR_CLASSPATH
#

my $ok = 0;
my $classpath = $ENV{'MTR_CLASSPATH'} || '';
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $jar;
my $notfound;
my $sep = (IS_WINDOWS) ? ';' : ':';


# -- look for the Connctor/J jar file
foreach $jar (split $sep, $classpath) {
  if($jar =~ m/mysql-connector/  && -f $jar) {
    $ok = 2;
    last;
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
  print $F "--let \$skip_test=1\n";
  print $F "--let \$skip_reason=JAR file not found: \"$notfound\"\n";
} 
else {
  print $F "--let \$skip_test=1\n";
  print $F "--let \$skip_reason=Please specify Connector/J JAR file in MTR_CLASSPATH env variable\n";
}

$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_connectorj_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/have_connectorj_result.inc

# skip if connectorj not found
if ($skip_test) {
  --skip $skip_reason
}
