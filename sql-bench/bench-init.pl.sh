#!/usr/bin/perl
# Copyright (c) 2000-2003, 2005-2007 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA
#
##########################################################
# this is the base file every test is using ....
# this is made for not changing every file if we want to
# add an option or just want to change something in
# code what is the same in every file ...
##########################################################

#
# The exported values are:

# $opt_...	Various options
# $date		Current date in ISO format
# $server	Object for current server
# $limits	Hash reference to limits for benchmark

$benchmark_version="2.15";
use Getopt::Long;
use POSIX;

require "$pwd/server-cfg" || die "Can't read Configuration file: $!\n";

$|=1;				# Output data immediately

$opt_skip_test=$opt_skip_create=$opt_skip_delete=$opt_verbose=$opt_fast_insert=$opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=$opt_log=$opt_use_old_results=$opt_help=$opt_odbc=$opt_small_test=$opt_small_tables=$opt_samll_key_tables=$opt_stage=$opt_old_headers=$opt_die_on_errors=$opt_tcpip=$opt_random=$opt_only_missing_tests=$opt_temporary_tables=0;
$opt_cmp=$opt_user=$opt_password=$opt_connect_options=$opt_connect_command= "";
$opt_server="mysql"; $opt_dir="output";
$opt_host="localhost";$opt_database="test";
$opt_machine=""; $opt_suffix="";
$opt_create_options=undef;
$opt_optimization="None";
$opt_hw="";
$opt_threads=-1;

if (!defined($opt_time_limit))
{
  $opt_time_limit=10*60;	# Don't wait more than 10 min for some tests
}

$log_prog_args=join(" ", skip_arguments(\@ARGV,"comments","cmp","server",
					"user", "host", "database", "password",
					"use-old-results","skip-test",
					"optimization","hw",
					"machine", "dir", "suffix", "log"));
GetOptions("skip-test=s","comments=s","cmp=s","server=s","user=s","host=s","database=s","password=s","loop-count=i","row-count=i","skip-create","skip-delete","verbose","fast-insert","lock-tables","debug","fast","force","field-count=i","regions=i","groups=i","time-limit=i","log","use-old-results","machine=s","dir=s","suffix=s","help","odbc","small-test","small-tables","small-key-tables","stage=i","threads=i","random","old-headers","die-on-errors","create-options=s","hires","tcpip","silent","optimization=s","hw=s","socket=s","connect-options=s","connect-command=s","only-missing-tests","temporary-tables") || usage();

usage() if ($opt_help);
$server=get_server($opt_server,$opt_host,$opt_database,$opt_odbc,
                   machine_part(), $opt_socket, $opt_connect_options);
$limits=merge_limits($server,$opt_cmp);
$date=date();
@estimated=(0.0,0.0,0.0);		# For estimated time support

if ($opt_threads != -1)
{
    print "WARNING: Option --threads is deprecated and has no effect\n"
}

if ($opt_hires)
{
  eval "use Time::HiRes;";
}

{
  my $tmp= $opt_server;
  $tmp =~ s/_odbc$//;
  if (length($opt_cmp) && index($opt_cmp,$tmp) < 0)
  {
    $opt_cmp.=",$tmp";
  }
}
$opt_cmp=lc(join(",",sort(split(',',$opt_cmp))));

#
# set opt_lock_tables if one uses --fast and drivers supports it
#

if (($opt_lock_tables || $opt_fast) && $server->{'limits'}->{'lock_tables'})
{
  $opt_lock_tables=1;
}
else
{
  $opt_lock_tables=0;
}
if ($opt_fast)
{
  $opt_fast_insert=1;
  $opt_suffix="_fast" if (!length($opt_suffix));
}

if ($opt_odbc)
{
   $opt_suffix="_odbc" if (!length($opt_suffix));
}

if (!$opt_silent)
{
  print "Testing server '" . $server->version() . "' at $date\n\n";
}

if ($opt_debug)
{
  print "\nCurrent limits: \n";
  foreach $key (sort keys %$limits)
  {
    print $key . " " x (30-length($key)) . $limits->{$key} . "\n";
  }
  print "\n";
}

#
# Some help functions
#

sub skip_arguments
{
  my($argv,@skip_args)=@_;
  my($skip,$arg,$name,@res);

  foreach $arg (@$argv)
  {
    if ($arg =~ /^\-+([^=]*)/)
    {
      $name=$1;
      foreach $skip (@skip_args)
      {
	if (index($skip,$name) == 0)
	{
	  $name="";		# Don't use this parameters
	  last;
	}
      }
      push (@res,$arg) if (length($name));
    }
  }
  return @res;
}


sub merge_limits
{
  my ($server,$cmp)= @_;
  my ($name,$tmp_server,$limits,$res_limits,$limit,$tmp_limits);

  $res_limits=$server->{'limits'};
  if ($cmp)
  {
    foreach $name (split(",",$cmp))
    {
      $tmp_server= (get_server($name,$opt_host, $opt_database,
			       $opt_odbc,machine_part())
		    || die "Unknown SQL server: $name\n");
      $limits=$tmp_server->{'limits'};
      %new_limits=();
      foreach $limit (keys(%$limits))
      {
	if (defined($res_limits->{$limit}) && defined($limits->{$limit}))
	{
	  $new_limits{$limit}=min($res_limits->{$limit},$limits->{$limit});
	}
      }
      %tmp_limits=%new_limits;
      $res_limits=\%tmp_limits;
    }
  }
  return $res_limits;
}

sub date
{
  my ($sec, $min, $hour, $mday, $mon, $year) = localtime(time());
  sprintf("%04d-%02d-%02d %2d:%02d:%02d",
	  1900+$year,$mon+1,$mday,$hour,$min,$sec);
}

sub min
{
  my($min)=$_[0];
  my($i);
  for ($i=1 ; $i <= $#_; $i++)
  {
    $min=$_[$i] if ($min > $_[$i]);
  }
  return $min;
}

sub max
{
  my($max)=$_[0];
  my($i);
  for ($i=1 ; $i <= $#_; $i++)
  {
    $max=$_[$i] if ($max < $_[$i]);
  }
  return $max;
}


#
# Execute many statements in a row
#

sub do_many
{
  my ($dbh,@statements)=@_;
  my ($statement,$sth);

  foreach $statement (@statements)
  {
    if (!($sth=$dbh->do($statement)))
    {
      die "Can't execute command '$statement'\nError: $DBI::errstr\n";
    }
  }
}

sub safe_do_many
{
  my ($dbh,@statements)=@_;
  my ($statement,$sth);

  foreach $statement (@statements)
  {
    if (!($sth=$dbh->do($statement)))
    {
      print STDERR "Can't execute command '$statement'\nError: $DBI::errstr\n";
      return 1;
    }
  }
  return 0;
}



#
# Do a query and fetch all rows from a statement and return the number of rows
#

sub fetch_all_rows
{
  my ($dbh,$query,$must_get_result)=@_;
  my ($count,$sth);
  $count=0;

  print "$query: " if ($opt_debug);
  if (!($sth= $dbh->prepare($query)))
  {
    print "\n" if ($opt_debug);
    die "Error occured with prepare($query)\n -> $DBI::errstr\n";
    return undef;
  }
  if (!$sth->execute)
  {
    print "\n" if ($opt_debug);
    if (defined($server->{'error_on_execute_means_zero_rows'}) &&
       !$server->abort_if_fatal_error())
    {
      if (defined($must_get_result) && $must_get_result)
      {
	die "Error: Query $query didn't return any rows\n";
      }
      $sth->finish;
      print "0\n" if ($opt_debug);
      return 0;
    }
    die "Error occured with execute($query)\n -> $DBI::errstr\n";
    $sth->finish;
    return undef;
  }
  while ($sth->fetchrow_arrayref)
  {
    $count++;
  }
  print "$count\n" if ($opt_debug);
  if (defined($must_get_result) && $must_get_result && !$count)
  {
    die "Error: Query $query didn't return any rows\n";
  }
  $sth->finish;
  undef($sth);
  return $count;
}

sub do_query
{
  my($dbh,$query)=@_;
  print "$query\n" if ($opt_debug);
  $dbh->do($query) or
    die "\nError executing '$query':\n$DBI::errstr\n";
}

#
# Run a query X times
#

sub time_fetch_all_rows
{
  my($test_text,$result_text,$query,$dbh,$test_count)=@_;
  my($i,$loop_time,$end_time,$count,$rows,$estimated);

  print $test_text . "\n"   if (defined($test_text));
  $count=$rows=0;
  $loop_time=new Benchmark;
  for ($i=1 ; $i <= $test_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,$query) or die $DBI::errstr;
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i,
					   $test_count));
  }
  $end_time=new Benchmark;
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for $result_text ($count:$rows) " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}


#
# Handle estimated time of the server is too slow
# Returns 0 if one should continue as normal
#

sub predict_query_time
{
  my ($loop_time,$end_time,$count_ref,$loop,$loop_count)= @_;
  my ($k,$tmp);

  if (($end_time->[0] - $loop_time->[0]) > $opt_time_limit)
  {
    # We can't wait until the SUN dies.  Try to predict the end time
    if ($loop != $loop_count)
    {
      $tmp=($end_time->[0] - $loop_time->[0]);
      print "Note: Query took longer then time-limit: $opt_time_limit\nEstimating end time based on:\n";
      print "$$count_ref queries in $loop loops of $loop_count loops took $tmp seconds\n";
      for ($k=0; $k < 3; $k++)
      {
	$tmp=$loop_time->[$k]+($end_time->[$k]-$loop_time->[$k])/$loop*
	  $loop_count;
	$estimated[$k]+=($tmp-$end_time->[$k]);
	$end_time->[$k]=$tmp;
      }
      $$count_ref= int($$count_ref/$loop*$loop_count);
      return 1;
    }
  }
  return 0;
}

#
# standard end of benchmark
#

sub end_benchmark
{
  my ($start_time)=@_;

  $end_time=new Benchmark;
  if ($estimated[0])
  {
    print "Estimated total time: ";
    $end_time->[0]+=$estimated[0];
    $end_time->[1]+=$estimated[1];
    $end_time->[2]+=$estimated[2];
  }
  else
  {
    print "Total time: "
    }
  print timestr(timediff($end_time, $start_time),"all") . "\n";
  exit 0;
}

sub print_time
{
  my ($estimated)=@_;
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
}

#
# Create a filename part for the machine that can be used for log file.
#

sub machine_part
{
  my ($name,$orig);
  return $opt_machine if (length($opt_machine)); # Specified by user
# Specified by user
  $orig=$name=machine();
  $name="win9$1" if ($orig =~ /win.*9(\d)/i);
  $name="NT_$1" if ($orig =~ /Windows NT.*(\d+\.\d+)/i);
  $name="win2k" if ($orig =~ /Windows 2000/i);
  $name =~ s/\s+/_/g;		# Make the filenames easier to parse
  $name =~ s/-/_/g;
  $name =~ s/\//_/g;
  return $name;
}

sub machine
{
  my @name = POSIX::uname();
  my $name= $name[0] . " " . $name[2] . " " . $name[4];
  return $name;
}

#
# Usage
#

sub usage
{
    print <<EOF;
The MySQL benchmarks Ver $benchmark_version

All benchmarks takes the following options:

--comments
  Add a comment to the benchmark output.  Comments should contain
  extra information that 'uname -a' doesn\'t give and if the database was
  stared with some specific, non default, options.

--cmp=server[,server...]
  Run the test with limits from the given servers.  If you run all servers
  with the same --cmp, you will get a test that is comparable between
  the different sql servers.

--create-options=#
  Extra argument to all create statements.  If you for example want to
  create all MySQL tables as InnoDB tables use:
  --create-options=ENGINE=InnoDB

--temporary-tables
  Use temporary tables for all tests.

--database (Default $opt_database)
  In which database the test tables are created.

--debug
  This is a test specific option that is only used when debugging a test.
  Print out debugging information.

--dir (Default $opt_dir)
  Option to 'run-all-tests' to where the test results should be stored.

--fast
  Allow the database to use non standard ANSI SQL commands to make the
  test go faster.

--fast-insert
  Use "insert into table_name values(...)" instead of
  "insert into table_name (....) values(...)"
  If the database supports it, some tests uses multiple value lists.

--field-count
  This is a test specific option that is only used when debugging a test.
  This usually means how many fields there should be in the test table.

--force
  This is a test specific option that is only used when debugging a test.
  Continue the test even if there is some error.
  Delete tables before creating new ones.

--groups (Default $opt_groups)
  This is a test specific option that is only used when debugging a test.
  This usually means how many different groups there should be in the test.

--lock-tables
  Allow the database to use table locking to get more speed.

--log
  Option to 'run-all-tests' to save the result to the '--dir' directory.

--loop-count (Default $opt_loop_count)
  This is a test specific option that is only used when debugging a test.
  This usually means how many times times each test loop is executed.

--help
  Shows this help

--host='host name' (Default $opt_host)
  Host name where the database server is located.

--machine="machine or os_name"
  The machine/os name that is added to the benchmark output filename.
  The default is the OS name + version.

--odbc
  Use the ODBC DBI driver to connect to the database.

--only-missing-tests
  Only run test that don\'t have an old test result.
  This is useful when you want to do a re-run of tests that failed in last run.

--optimization='some comments'
 Add coments about optimization of DBMS, which was done before the test.

--password='password'
  Password for the current user.

--socket='socket'
  If the database supports connecting through a Unix socket,
  then use this socket to connect

--regions
  This is a test specific option that is only used when debugging a test.
  This usually means how AND levels should be tested.

--old-headers
  Get the old benchmark headers from the old RUN- file.

--server='server name'  (Default $opt_server)
  Run the test on the given SQL server.
  Known servers names are: Access, Adabas, AdabasD, Empress, Oracle,
  Informix, DB2, mSQL, MS-SQL, MySQL, Pg, Solid and Sybase

--silent
  Don't print info about the server when starting test.

--skip-delete
  This is a test specific option that is only used when debugging a test.
  This will keep the test tables after the test is run.

--skip-test=test1[,test2,...]
  For run-all-programs;  Don\'t execute the named tests.

--small-test
  This runs some tests with smaller limits to get a faster test.
  Can be used if you just want to verify that the database works, but
  don't have time to run a full test.

--small-tables
  This runs some tests that generate big tables with fewer rows.
  This can be used with databases that can\'t handle that many rows
  because of pre-sized partitions.

--suffix (Default $opt_suffix)
  The suffix that is added to the database name in the benchmark output
  filename.  This can be used to run the benchmark multiple times with
  different server options without overwritten old files.
  When using --fast the suffix is automaticly set to '_fast'.

--random
  Inform test suite that we are generate random inital values for sequence of
  test executions. It should be used for imitation of real conditions.

--threads=#  **DEPRECATED**
  This option has no effect, and will be removed in a future version.

--tcpip
  Inform test suite that we are using TCP/IP to connect to the server. In
  this case we can\t do many new connections in a row as we in this case may
  fill the TCP/IP stack

--time-limit (Default $opt_time_limit)
  How long a test loop is allowed to take, in seconds, before the end result
  is 'estimated'.

--use-old-results
  Option to 'run-all-tests' to use the old results from the  '--dir' directory
  instead of running the tests.

--user='user_name'
  User name to log into the SQL server.

--verbose
  This is a test specific option that is only used when debugging a test.
  Print more information about what is going on.

--hw='some comments'
 Add coments about hardware used for this test.

--connect-options='some connect options'
  Add options, which uses at DBI connect.
  For example --connect-options=mysql_read_default_file=/etc/my.cnf.

--connect-command='SQL command'
  Initialization command to execute when logged in. Useful for setting
  up the environment.

EOF
  exit(0);
}



####
#### The end of the base file ...
####
1;
