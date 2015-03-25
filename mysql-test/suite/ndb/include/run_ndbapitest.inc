--perl
use strict;
use lib "lib/";
use My::Find;
use My::Exec;
use File::Basename;
use IO::File;

my $name = $ENV{NDBAPITEST_NAME} or die "Need NDBAPITEST_NAME";
my $args = $ENV{NDBAPITEST_ARGS};
my $max_lines = $ENV{NDBAPITEST_MAXLINES} || 200;

my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR
my $test_bin = my_find_bin($bindir,
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
my $res = exec_print_on_error($cmd, $max_lines);
if ($res)
{
  # Test program suceeded
  print $F "exit;\n";
  exit(0);
}
else
{
  print $F "die Test program failed!;\n";
}
$F->close(); 

EOF

if ($NDBAPITEST_CLEANUP_QUERY != '')
{
  eval $NDBAPITEST_CLEANUP_QUERY;
}

--source $MYSQLTEST_VARDIR/tmp/run_ndbapitest.result
