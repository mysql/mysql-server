#!@PERL@
# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# This program runs all test that starts with 'test-' and sums
# the results that the program prints.
# Each time result should be of the form:
# Time for|to KEYWORD (number_of_runs) 'other info': timestr()
#
# All options to this script is passed to all test program.
# useful options:
# --fast --force --lock-tables
# --server   ==> mysql (default) / mSQL / Pg (postgres) / Solid
# --user     ==> the user with permission to create / drop / select
# --pass     ==> password for the user
# --cmp      ==> Compare --server with one of the others (mysql/mSQL/Pg/Solid)
# --comments ==> everything you want to say such as the extra options you
#                gave to the db server. (use --comments="xxx xxx xxx"
# --machine  ==> Give a OS/machine id for your logfiles.
# --log	     ==> puts output in output/RUN-server-machine-cmp-$opt_cmp

use DBI;

$opt_silent=1;			# Don't write header

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";
$opt_silent=0;
$perl=$^X;
$machine=machine();
$redirect= !($machine =~ /windows/i || $machine =~ "^NT\s") ? "2>&1" : "";
$dir= ($pwd =~ /\\/) ? '\\' : '/';	# directory symbol for shell

$prog_args="";
foreach $arg (@ARGV)
{
  if ($redirect)
  {
    $prog_args.="'" . $arg . "' ";
  }
  else
  {
    # Windows/NT can't handle ' around arguments
    $prog_args.=$arg . " ";    
  }
}

$prog_count=$errors=0;

if ($opt_cmp) {
	$filename = "$opt_server$opt_suffix-" . machine_part() . "-cmp-$opt_cmp";
} else {
	$filename = "$opt_server$opt_suffix-" . machine_part();
}

if (! -d $opt_dir)
{
  if (-e $opt_dir)
  {
    die "$opt_dir isn't a directory\n";
  }
  mkdir $opt_dir,0777 || die "Can't create directory: $opt_dir\n";
}

if ($opt_skip_test) {
  (@skip_tests) = split(/,\s*/, $opt_skip_test);
}

if ($opt_old_headers)
{
  read_headers("$opt_dir/RUN-$filename");
}
else
{
  $server_version=$server->version();
}

if (!$opt_log)
{
  open(LOG,">&STDOUT");
}
else
{
  open(LOG, "> $opt_dir/RUN-$filename") ||
    die "Can't write to $opt_dir/RUN-$filename: $!\n";
}

select(LOG);
$|=1;

print "Benchmark DBD suite: $benchmark_version\n";
print "Date of test:        $date\n";
print "Running tests on:    $machine\n";
print "Arguments:           $log_prog_args\n";
print "Comments:            $opt_comments\n";
print "Limits from:         $opt_cmp\n";
print "Server version:      $server_version\n\n";


$estimated=$warning=$got_warning=0;
while (<test-*>)
{
  next if (/\.sh$/);		# configure script
  next if (/\-fork$/);		# test script
  $prog_count++;
  /test-(.*)$/;			# Remove test from name
  $prog=$1;
  $skip_prog = 0;
  foreach $skip_this (@skip_tests) {
    if ($prog =~ /$skip_this/i) {
      $skip_prog = 1;
      last;
    }
  }
  print "$prog: ";
  if ((!$opt_use_old_results) && (!$skip_prog))
  {
    if (system("$perl ./test-$prog $prog_args > \"$opt_dir$dir$prog-$filename\" $redirect"))
    {
      printf STDERR "Warning: Can't execute $prog.  Check the file '$opt_dir$dir$prog-$filename'\n";
      die "aborted" if ($opt_die_on_errors);
    }
  }
  open(TEST,"$opt_dir/$prog-$filename");
  $last_line="";
  while(<TEST>)
  {
    chomp;
    $last_line=$_ if (!(/^\s*$/));		# Search after last line
  }
  if ($last_line =~ /Total time:/i)
  {
    print $last_line . "\n";
    open(TEST,"$opt_dir/$prog-$filename");
    while (<TEST>)
    {
      if (/^(estimated |)time (to|for) ([^\s:]*)\s*\((\d*)(:\d*)*\)[^:]*:\s*([\d.]+) .*secs \(\s*([^\s]*) usr\s*\+*\s*([^\s]*) sys.*=\s+([\d.]*)\s+cpu/i)
      {
	$arg=$summa{$3};
	if (!defined($arg))
	{
	  $summa{$3}= [ $4,$6,$7,$8,$9,""];
	}
	else
	{
	  $arg->[0]+=$4;
	  $arg->[1]+=$6;
	  $arg->[2]+=$7;
	  $arg->[3]+=$8;
	  $arg->[4]+=$9;
	}
	$prog_sum[0]+=$4;
	$prog_sum[1]+=$6;
	$prog_sum[2]+=$7;
	$prog_sum[3]+=$8;
	$prog_sum[4]+=$9;
	if (length($1))
	{
	  $summa{$3}->[5].="+";
	  $estimated=1;
	}
	if ($got_warning)
	{
	  $summa{$3}->[5].="?";
	  $warning=1;
	  $got_warning=0;
	}
      }
      elsif (/^warning/i)
      {
	$got_warning=1;
      }
      else
      {
	$got_warning=0;
      }
    }
    if ($opt_debug)
    {
      print "Summary for $prog: ", join(" ",@prog_sum), "\n";
    }
  }
  else
  {
    $errors++;
    print "Failed ($opt_dir/$prog-$filename)\n";
  }
}

print "\n";
if (!$errors)
{
  print "All $prog_count test executed successfully\n";
}
else
{
  print "Of $prog_count tests, $errors tests didn't work\n";
}
if ($estimated)
{
  print "Tests with estimated time have a + at end of line\n"
}
if ($warning)
{
  print "Tests with didn't return the correct result have a ? at end of line\n";
}

if (%summa)
{
  @total=(0,0,0,0,0,"");
  print "\nTotals per operation:\n";
  print "Operation             seconds     usr     sys     cpu   tests\n";
  foreach $key (sort(keys %summa))
  {
    $arg=$summa{$key};
    printf("%-35.35s %7.2f %7.2f %7.2f %7.2f %7d %s\n",
	   $key,$arg->[1],$arg->[2],$arg->[3],$arg->[4],$arg->[0],
	   $arg->[5]);

    for ($i=0 ; $i < 5 ; $i++)
    {
      $total[$i]+=$arg->[$i];
    }
    $total[5].=$arg->[$i];
  }
  printf("%-35.35s %7.2f %7.2f %7.2f %7.2f %7d %s\n",
	 "TOTALS",$total[1],$total[2],$total[3],$total[4],$total[0],
	 $total[5]);
}

select(STDOUT);
if ($opt_log)
{
  print "Test finished. You can find the result in:\n$opt_dir/RUN-$filename\n";
}


#
# Read headers from an old benchmark run
#

sub read_headers
{
  my ($filename)=@_;

  # Clear current values
  $benchmark_version=$date=$machine=$server_version="";

  open(TMP, "<$filename") || die "Can't open $filename\n";
  while (<TMP>)
  {
    chop;
    if (/^Benchmark DBD.*:\s+(.*)$/)
    {
      $benchmark_version=$1;
    }
    elsif (/^Date of.*:\s+(.*)/)
    {
      $date=$1;
    }
    elsif (/^Running.*:\s+(.*)$/)
    {
      $machine=$1;
    }
    elsif (/^Arguments.*:\s+(.*)$/)
    {
      $log_prog_args=$1;
    }
    elsif (/^Comments.*:\s+(.*)$/)
    {
      $opt_comments=$1;
    }
    elsif (/^Limits.*:\s+(.*)$/)
    {
      $opt_cmp=$1;
    }
    elsif (/^Server ver.*:\s+(.*)$/)
    {
      $server_version=$1;
    }
  }
  close(TMP);
}
