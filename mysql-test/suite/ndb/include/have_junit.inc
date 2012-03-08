--perl
use strict;
use IO::File;
use lib "lib/";
use My::Platform;

my $ok = 0;
my $classpath = $ENV{'MTR_CLASSPATH'} || '';
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $jar;
my $notfound;
my $sep = (IS_WINDOWS) ? ';' : ':';

# -- look for the junit jar file
foreach $jar (split $sep, $classpath) {
  if($jar =~ m/junit/  && -f $jar) {
    $ok = 2;
    last;
  }
  elsif($jar =~ m/junit/) {
    $ok = 1;
    $notfound = $jar;
  }
}

# -- write the result to the output file
my $F = IO::File->new("$vardir/tmp/have_junit_result.inc", "w") or die;
if($ok == 2) {
  print $F  "--echo Found JUnit JAR in MTR_CLASSPATH\n";
}
elsif($ok == 1) {
  print $F  "--skip JAR file not found: \"$notfound\"\n";
} 
else {
  print $F  "--skip Please specify Junit JAR file in MTR_CLASSPATH env variable\n";
}

$F->close();

EOF

--source $MYSQLTEST_VARDIR/tmp/have_junit_result.inc
