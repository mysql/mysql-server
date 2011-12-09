#!/usr/bin/perl
# Copyright (C) 2000-2001, 2003 MySQL AB
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
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# This test is for testing the speed of connections and sending
# data to the client.
#
# By changing the variable '$opt_loop_count' value you can make this test
# easier/harderto your computer to execute. You can also change this value
# by using option --loop_value='what_ever_you_like'.
##################### Standard benchmark inits ##############################

use Cwd;
use DBI;
use Benchmark;

$opt_loop_count=100000;	# Change this to make test harder/easier
$str_length=65000;	# This is the length of blob strings in PART:5
$max_test=20;		# How many times to test if the server is busy

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

# This is the length of blob strings in PART:5
$str_length=min($limits->{'max_text_size'},$limits->{'query_size'}-30,$str_length);

if ($opt_small_test)
{
  $opt_loop_count/=100;
}

$opt_loop_count=min(1000, $opt_loop_count) if ($opt_tcpip);
$small_loop_count=$opt_loop_count/10; # For connect tests

print "Testing the speed of connecting to the server and sending of data\n";
print "Connect tests are done $small_loop_count times and other tests $opt_loop_count times\n\n";

################################# PART:1 ###################################
####
####  Start timeing and start connect test..
####

$start_time=new Benchmark;

print "Testing connection/disconnect\n";

$loop_time=new Benchmark;
$errors=0;

for ($i=0 ; $i < $small_loop_count ; $i++)
{
  print "$i " if (($opt_debug));
  for ($j=0; $j < $max_test ; $j++)
  {
    if ($dbh = DBI->connect($server->{'data_source'}, $opt_user,
			    $opt_password))
    {
      $dbh->disconnect;
      last;
    }
    select(undef, undef, undef, 0.01*$j);
    print "$errors " if (($opt_debug));
    $errors++;
  }
  die "Got error '$DBI::errstr' after $i connects" if ($j == $max_test);
  $dbh->disconnect;
  undef($dbh);
}
$end_time=new Benchmark;
print "Warning: $errors connections didn't work without a time delay\n" if ($errors);
print "Time to connect ($small_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

################################# PART:2 ###################################
#### Now we shall do first one connect, then simple select
#### (select 1..) and then close connection. This will be
#### done $small_loop_count times.

if ($limits->{'select_without_from'})
{
  print "Test connect/simple select/disconnect\n";
  $loop_time=new Benchmark;

  for ($i=0; $i < $small_loop_count; $i++)
  {
    $dbh = DBI->connect($server->{'data_source'}, $opt_user, $opt_password) || die $DBI::errstr;
    $sth = $dbh->do("select $i") or die $DBI::errstr;
    $dbh->disconnect;
  }
  $end_time=new Benchmark;
  print "Time for connect+select_simple ($small_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

################################# PART:3 ###################################
####
#### Okay..Next thing we'll do is a simple select $opt_loop_count times.
####

$dbh = DBI->connect($server->{'data_source'}, $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

if ($limits->{'select_without_from'})
{
  print "Test simple select\n";
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth = $dbh->do("select $i") or die $DBI::errstr;
  }
  $end_time=new Benchmark;
  print "Time for select_simple ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

###########################################################################
#### The same as the previous test, but always execute the same select
#### This is done to test the query cache for real simple selects.

if ($limits->{'select_without_from'})
{
  print "Test simple select\n";
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth = $dbh->do("select 10000") or die $DBI::errstr;
  }
  $end_time=new Benchmark;
  print "Time for select_simple_cache ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

##########################################################################
#### First, we'll create a simple table 'bench1'
#### Then we shall do $opt_loop_count selects from this table.
#### Table will contain very simple data.

$sth = $dbh->do("drop table bench1" . $server->{'drop_attr'});
do_many($dbh,$server->create("bench1",
			     ["a int NOT NULL",
			      "i int",
			      "s char(10)"],
			     ["primary key (a)"]));
$sth = $dbh->do("insert into bench1 values(1,100,'AAA')") or die $DBI::errstr;

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}
$dbh->disconnect;

#
# First test connect/select/disconnect
#
print "Testing connect/select 1 row from table/disconnect\n";

$loop_time=new Benchmark;
$errors=0;

for ($i=0 ; $i < $small_loop_count ; $i++)
{
  for ($j=0; $j < $max_test ; $j++)
  {
    last if ($dbh = DBI->connect($server->{'data_source'}, $opt_user, $opt_password));
    $errors++;
  }
  die $DBI::errstr if ($j == $max_test);

  $sth = $dbh->do("select a,i,s,$i from bench1") # Select * from table with 1 record
    or die $DBI::errstr;
  $dbh->disconnect;
}

$end_time=new Benchmark;
print "Warning: $errors connections didn't work without a time delay\n" if ($errors);
print "Time to connect+select_1_row ($small_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

#
# The same test, but without connect/disconnect
#
print "Testing select 1 row from table\n";

$dbh = $server->connect();
$loop_time=new Benchmark;

for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $sth = $dbh->do("select a,i,s,$i from bench1") # Select * from table with 1 record
    or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time to select_1_row ($opt_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

#
# Same test (as with one row) but now with a cacheable query
#

$loop_time=new Benchmark;

for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $sth = $dbh->do("select a,i,s from bench1") # Select * from table with 1 record
    or die $DBI::errstr;
}
$end_time=new Benchmark;
print "Time to select_1_row_cache ($opt_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

#
# The same test, but with 2 rows (not cacheable).
#

print "Testing select 2 rows from table\n";

$sth = $dbh->do("insert into bench1 values(2,200,'BBB')")
  or die $DBI::errstr;

$loop_time=new Benchmark;

for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $sth = $dbh->do("select a,i,s,$i from bench1") # Select * from table with 2 record
    or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time to select_2_rows ($opt_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

#
# Simple test to test speed of functions.
#

if ($limits->{'functions'})
{
  print "Test select with aritmetic (+)\n";
  $loop_time=new Benchmark;

  for ($i=0; $i < $opt_loop_count; $i++)
  {
    $sth = $dbh->do("select a+a+a+a+a+a+a+a+a+$i from bench1") or die $DBI::errstr;
  }
  $end_time=new Benchmark;
  print "Time for select_column+column ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

$sth = $dbh->do("drop table bench1" . $server->{'drop_attr'})
  or die $DBI::errstr;

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

################################# PART:5 ###################################
#### We'll create one table with a single blob field,but with a
#### huge record in it and then we'll do $opt_loop_count selects
#### from it.

goto skip_blob_test if (!$limits->{'working_blobs'});

print "Testing retrieval of big records ($str_length bytes)\n";

do_many($dbh,$server->create("bench1", ["b blob"], []));
$dbh->{LongReadLen}= $str_length; # Set retrieval buffer

my $string=(A) x ($str_length); # This will make a string $str_length long.
$sth = $dbh->prepare("insert into bench1 values(?)") or die $dbh->errstr;
$sth->execute($string) or die $sth->errstr;
undef($string);
if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

$loop_time=new Benchmark;

for ($i=0 ; $i < $small_loop_count ; $i++)
{
  $sth = $dbh->prepare("select b,$i from bench1");
  if (!$sth->execute || !(@row = $sth->fetchrow_array) ||
      length($row[0]) != $str_length)
  {
    warn "$DBI::errstr - ".length($row[0])." - $str_length **\n";
  }
  $sth->finish;
}

$end_time=new Benchmark;
print "Time to select_big_str ($small_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

$sth = $dbh->do("drop table bench1" . $server->{'drop_attr'})
  or do
{
    # Fix for Access 2000
    die $dbh->errstr if (!$server->abort_if_fatal_error());
};

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

skip_blob_test:

################################ END ###################################
####
#### End of the test...Finally print time used to execute the
#### whole test.

$dbh->disconnect;
end_benchmark($start_time);
