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
		           ["runtime_output_directory", "bin"],
                           $name, NOT_REQUIRED);

my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $F = IO::File->new("$vardir/tmp/run_ndbapitest_result.inc", "w") or die;
unless($test_bin)
{
  print $F "--let \$skip_test = 1\n";
  print $F "--let \$skip_reason = skip Could not find $name\n";
  exit(0);
}

if ($ENV{'MYSQL_TMP_DIR'})
{
  $ENV{'NDBT_TMP_DIR'} = $ENV{'MYSQL_TMP_DIR'};
}

my $cmd = $test_bin;
$cmd .= " $args" if $args;
my $res = exec_print_on_error($cmd, $max_lines);
if (!$res)
{
  # No need to remove run_ndbapitest_result.inc when test case dies. Because,
  # when it dies MTR fails and returns without executing check-testcase.test.

  print $F "die Test program failed!;\n";
}
$F->close(); 

EOF

if ($NDBAPITEST_CLEANUP_QUERY != '')
{
  eval $NDBAPITEST_CLEANUP_QUERY;
}

--source $MYSQLTEST_VARDIR/tmp/run_ndbapitest_result.inc
--remove_file $MYSQLTEST_VARDIR/tmp/run_ndbapitest_result.inc

# When we --source run_ndbapitest_result.inc, if test is skipped,
# test case will return immediately and will not execute other commands
# after --source. So, remove file will not get executed and check test
# case will fail. Hence, moving --skip outside run_ndbapitest_result.inc.

if ($skip_test)
{
  --skip $skip_reason
}
