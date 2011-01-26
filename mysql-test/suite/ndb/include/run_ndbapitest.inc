--perl
use strict;
use lib "lib/";
use My::Find;
use File::Basename;
use IO::File;

my $name = $ENV{NDBAPITEST_NAME} or die "Need NDBAPITEST_NAME";
my $args = $ENV{NDBAPITEST_ARGS};
my $max_lines = $ENV{NDBAPITEST_MAXLINES} || 200;
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $test_bin = my_find_bin($basedir,
		           ["storage/ndb/test/ndbapi", "bin"],
                           $name, NOT_REQUIRED);

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/run_ndbapitest.result", "w") or die;
unless($test_bin)
{
  print $F "skip Could not find $name;\n";
  exit(0);
}

if ($ENV{'MYSQL_TMP_DIR'})
{
  $ENV{'NDBT_TMP_DIR'} = $ENV{'MYSQL_TMP_DIR'};
}

my $cmd = $test_bin;
$cmd .= " $args" if $args;
$cmd .= " 2>&1";
print "Running '$cmd'\n";
my @output = `$cmd`;
if ($? == 0)
{
  # Test program suceeded
  print $F "exit;\n";
  exit(0);
}

print $F "die Test program failed!;\n";
$F->close(); 

if ($? == -1)
{
  # Failed to execute program
  print "Failed to execute: $!\n";
}
elsif ($?)
{
  # Test program failed
  my $sig = $? & 127;
  my $return = $? >> 8;
  print "Test program killed by signal $sig\n" if $sig;
  print "Test program failed with error $return\n" if $return;

  # Show the last lines of the output 
  my $lines = scalar(@output);
  $lines = $max_lines if $lines > $max_lines;  
  foreach my $line (splice(@output, -$lines))
  {
    print $line;
  }
}

EOF

--source $MYSQLTEST_VARDIR/tmp/run_ndbapitest.result

