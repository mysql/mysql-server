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
# This test is for testing how long it takes to create tables,
# make a count(*) from them and finally drop the tables. These
# commands will be done in different ways in this test.
# Using option --fast will drop all the tables in the end
# of this test with one command instead of making own
# 'drop' command for each and every table.
# By changing the variable '$table_amount' value you can make
# this test a lot harder/easier for your computer to drive.
# Note that when using value bigger than 64 for this variable
# will do 'drop table'-command	in totally different way because of that
# how MySQL handles these commands.

##################### Standard benchmark inits ##############################

use DBI;
use Benchmark;

$opt_loop_count=10000; # Change this to make test harder/easier
# This is the default value for the amount of tables used in this test.

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

if ($opt_small_test)
{
  $opt_loop_count/=100;
}

$max_tables=min($limits->{'max_tables'},$opt_loop_count);

print "Testing the speed of creating and droping tables\n";
print "Testing with $max_tables tables and $opt_loop_count loop count\n\n";

####
####  Connect and start timeing
####

$dbh = $server->connect();

### Test how the database can handle many tables
### Create $max_tables ; Access all off them with a simple query
### and then drop the tables

if ($opt_force) # If tables used in this test exist, drop 'em
{
  print "Okay..Let's make sure that our tables don't exist yet.\n\n";
  for ($i=1 ; $i <= $max_tables ; $i++)
  {
    $dbh->do("drop table bench_$i" . $server->{'drop_attr'});
  }
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

print "Testing create of tables\n";

$loop_time=$start_time=new Benchmark;

for ($i=1 ; $i <= $max_tables ; $i++)
{
  if (do_many($dbh,$server->create("bench_$i",
				   ["i int NOT NULL",
				    "d double",
				    "f float",
				    "s char(10)",
				    "v varchar(100)"],
				   ["primary key (i)"])))
  {
    # Got an error; Do cleanup
    for ($i=1 ; $i <= $max_tables ; $i++)
    {
      $dbh->do("drop table bench_$i" . $server->{'drop_attr'});
    }
    die "Test aborted";
  }
}

$end_time=new Benchmark;
print "Time for create_MANY_tables ($max_tables): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#### Here comes $max_tables couples of cont(*) to the tables.
#### We'll check how long it will take...
####

print "Accessing tables\n";

if ($limits->{'group_functions'})
{
  $query="select count(*) from ";
  $type="select_group_when_MANY_tables";
}
else
{
  $query="select * from ";
  $type="select_when_MANY_tables";
}

$loop_time=new Benchmark;
for ($i=1 ; $i <= $max_tables ; $i++)
{
  $sth = $dbh->do("$query bench_$i") or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time to $type ($max_tables): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
#### Now we are going to drop $max_tables tables;
####

print "Testing drop\n";

$loop_time=new Benchmark;

if ($opt_fast && $server->{'limits'}->{'multi_drop'} &&
    $server->{'limits'}->{'query_size'} > 11+$max_tables*10)
{
  my $query="drop table bench_1";
  for ($i=2 ; $i <= $max_tables ; $i++)
  {
    $query.=",bench_$i";
  }
  $sth = $dbh->do($query . $server->{'drop_attr'}) or die $DBI::errstr;
}
else
{
  for ($i=1 ; $i <= $max_tables ; $i++)
  {
    $sth = $dbh->do("drop table bench_$i" . $server->{'drop_attr'})
      or die $DBI::errstr;
  }
}


$end_time=new Benchmark;
print "Time for drop_table_when_MANY_tables ($max_tables): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#### We'll do first one 'create table' and then we'll drop it
#### away immediately. This loop shall be executed $opt_loop_count
#### times.

print "Testing create+drop\n";

$loop_time=new Benchmark;

for ($i=1 ; $i <= $opt_loop_count ; $i++)
{
  do_many($dbh,$server->create("bench_$i",
			       ["i int NOT NULL",
				"d double",
				"f float",
				"s char(10)",
				"v varchar(100)"],
			       ["primary key (i)"]));
  $sth = $dbh->do("drop table bench_$i" . $server->{'drop_attr'}) or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for create+drop ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#
# Same test, but with a table with many keys
#

my @fields=(); my @keys=();
$keys=min($limits->{'max_index'},16);		# 16 is more than enough
$seg= min($limits->{'max_index_parts'},$keys,16);	# 16 is more than enough

# Make keys on the most important types
@types=(0,0,0,1,0,0,0,1,1,1,1,1,1,1,1,1,1);	# A 1 for each char field
push(@fields,"field1 tinyint not null");
push(@fields,"field2 mediumint not null");
push(@fields,"field3 smallint not null");
push(@fields,"field4 char(16) not null");
push(@fields,"field5 integer not null");
push(@fields,"field6 float not null");
push(@fields,"field7 double not null");
for ($i=8 ; $i <= $keys ; $i++)
{
  push(@fields,"field$i char(5) not null");	# Should be relatively fair
}

# Let first key contain many segments
my $query="primary key (";
for ($i= 1 ; $i <= $seg ; $i++)
{
  $query.= "field$i,";
}
substr($query,-1)=")";
push (@keys,$query);

#Create other keys
for ($i=2 ; $i <= $keys ; $i++)
{
  push(@keys,"index index$i (field$i)");
}

$loop_time=new Benchmark;
for ($i=1 ; $i <= $opt_loop_count ; $i++)
{
  do_many($dbh,$server->create("bench_$i", \@fields, \@index));
  $dbh->do("drop table bench_$i" . $server->{'drop_attr'}) or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for create_key+drop ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

####
#### End of benchmark
####

$dbh->disconnect;				# close connection
end_benchmark($start_time);
