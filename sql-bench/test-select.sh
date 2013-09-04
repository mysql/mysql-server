#!/usr/bin/perl
# Copyright (c) 2000, 2001, 2003, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
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
# Test of selecting on keys that consist of many parts
#
##################### Standard benchmark inits ##############################

use Cwd;
use DBI;
use Getopt::Long;
use Benchmark;

$opt_loop_count=10000;
$opt_medium_loop_count=1000;
$opt_small_loop_count=10;
$opt_regions=6;
$opt_groups=100;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

$columns=min($limits->{'max_columns'},500,($limits->{'query_size'}-50)/24,
	     $limits->{'max_conditions'}/2-3);

if ($opt_small_test)
{
  $opt_loop_count/=10;
  $opt_medium_loop_count/=10;
  $opt_small_loop_count/=10;
  $opt_groups/=10;
}

print "Testing the speed of selecting on keys that consist of many parts\n";
print "The test-table has $opt_loop_count rows and the test is done with $columns ranges.\n\n";

####
####  Connect and start timeing
####

$dbh = $server->connect();
$start_time=new Benchmark;

####
#### Create needed tables
####

goto select_test if ($opt_skip_create);

print "Creating table\n";
$dbh->do("drop table bench1" . $server->{'drop_attr'});

do_many($dbh,$server->create("bench1",
			     ["region char(1) NOT NULL",
			      "idn integer(6) NOT NULL",
			      "rev_idn integer(6) NOT NULL",
			      "grp integer(6) NOT NULL"],
			     ["primary key (region,idn)",
			      "unique (region,rev_idn)",
			      "unique (region,grp,idn)"]));
if ($opt_lock_tables)
{
  do_query($dbh,"LOCK TABLES bench1 WRITE");
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

####
#### Insert $opt_loop_count records with
#### region:	"A" -> "E"
#### idn: 	0 -> count
#### rev_idn:	count -> 0,
#### grp:	distributed values 0 - > count/100
####

print "Inserting $opt_loop_count rows\n";

$loop_time=new Benchmark;

if ($opt_fast && $server->{transactions})
{
  $dbh->{AutoCommit} = 0;
}

$query="insert into bench1 values (";
$half_done=$opt_loop_count/2;
for ($id=0,$rev_id=$opt_loop_count-1 ; $id < $opt_loop_count ; $id++,$rev_id--)
{
  $grp=$id*3 % $opt_groups;
  $region=chr(65+$id%$opt_regions);
  do_query($dbh,"$query'$region',$id,$rev_id,$grp)");
  if ($id == $half_done)
  {				# Test with different insert
    $query="insert into bench1 (region,idn,rev_idn,grp) values (";
  }
}

if ($opt_fast && $server->{transactions})
{
  $dbh->commit;
  $dbh->{AutoCommit} = 1;
}

$end_time=new Benchmark;
print "Time to insert ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_lock_tables)
{
  do_query($dbh,"UNLOCK TABLES");
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh,"bench1");
}

if ($opt_lock_tables)
{
  do_query($dbh,"LOCK TABLES bench1 WRITE");
}

####
#### Do some selects on the table
####

select_test:

if ($limits->{'group_functions'})
{
  my ($tmp); $tmp=1000;
  print "Test if the database has a query cache\n";

  # First ensure that the table is read into memory
  fetch_all_rows($dbh,"select sum(idn+$tmp),sum(rev_idn-$tmp) from bench1");

  $loop_time=new Benchmark;
  for ($tests=0 ; $tests < $opt_loop_count ; $tests++)
  {
    fetch_all_rows($dbh,"select sum(idn+100),sum(rev_idn-100) from bench1");
  }
  $end_time=new Benchmark;
  print "Time for select_cache ($opt_loop_count): " .
     timestr(timediff($end_time, $loop_time),"all") . "\n\n";

  # If the database has a query cache, the following loop should be much
  # slower than the previous loop

  $loop_time=new Benchmark;
  for ($tests=0 ; $tests < $opt_loop_count ; $tests++)
  {
    fetch_all_rows($dbh,"select sum(idn+$tests),sum(rev_idn-$tests) from bench1");
  }
  $end_time=new Benchmark;
  print "Time for select_cache2 ($opt_loop_count): " .
     timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}


print "Testing big selects on the table\n";
$loop_time=new Benchmark;
$rows=0;
for ($i=0 ; $i < $opt_small_loop_count ; $i++)
{
  $grp=$i*11 % $opt_groups;
  $region=chr(65+$i%($opt_regions+1));	# One larger to test misses
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region='$region'");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region='$region' and idn=$i");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region='$region' and rev_idn=$i");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region='$region' and grp=$grp");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region>='B' and region<='C' and grp=$grp");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where region>='B' and region<='E' and grp=$grp");
  $rows+=fetch_all_rows($dbh,"select idn from bench1 where grp=$grp"); # This is hard
}
$count=$opt_small_loop_count*7;

$end_time=new Benchmark;
print "Time for select_big ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

# Test select with many OR's

$loop_time=new Benchmark;
$tmpvar=0;
$count=0;
$estimated=0;
$max_and_conditions=$limits->{'max_conditions'}/2;
$rows=0;

for ($i=0 ; $i < $opt_small_loop_count ; $i++)
{
  $region=chr(65+$i%($opt_regions+1));	# One larger to test out-of-regions
  $query="select * from bench1 where ";
  $or_part="grp = 1";
  $or_part2="region='A' and grp=1";

  for ($j=1 ; $j < $columns; $j++)
  {
    $tmpvar^= ((($tmpvar + 63) + $j)*3 % 100000);
    $tmp=$tmpvar % $opt_groups;
    $tmp_region=chr(65+$tmpvar%$opt_regions);
    $or_part.=" or grp=$tmp";
    if ($j < $max_and_conditions)
    {
      $or_part2.=" or region='$tmp_region' and grp=$tmp";
    }
  }
  $or_part="region='$region' and ($or_part)";

# Same query, but use 'func_extra_in_num' instead.
  if ($limits->{'func_extra_in_num'})
  {
    $in_part=$or_part;
    $in_part=~ s/ = / IN \(/;
    $in_part=~ s/ or grp=/,/g;
    $in_part.= ")";
    defined($found=fetch_all_rows($dbh,$query . $in_part)) || die $DBI::errstr;
    $rows+=$found;
    $count++;
  }
  for ($j=0; $j < 10 ; $j++)
  {
    $rows+=fetch_all_rows($dbh,$query . $or_part);
    $rows+=fetch_all_rows($dbh,$query . $or_part2);
# Do it a little harder by setting a extra range
    $rows+=fetch_all_rows($dbh,"$query ($or_part) and idn < 50");
    $rows+=fetch_all_rows($dbh,"$query (($or_part) or (region='A' and grp < 10)) and region <='B'")
  }
  $count+=$j*4;
  $end_time=new Benchmark;
  last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					 $opt_small_loop_count));
}

print_time($estimated);
print " for select_range ($count:$rows): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";

#
# Testing MIN() and MAX() on keys
#

if ($limits->{'group_functions'} && $limits->{'order_by_unused'})
{
  $loop_time=new Benchmark;
  $count=0;
  $estimated=0;
  for ($tests=0 ; $tests < $opt_loop_count ; $tests++)
  {
    $count+=7;
    $grp=$tests*3 % $opt_groups;
    $region=chr(65+$tests % $opt_regions);
    if ($limits->{'group_func_sql_min_str'})
    {
      fetch_all_rows($dbh,"select min(region) from bench1");
      fetch_all_rows($dbh,"select max(region) from bench1");
      fetch_all_rows($dbh,"select min(region),max(region) from bench1");
    }
    fetch_all_rows($dbh,"select min(rev_idn) from bench1 where region='$region'");

    fetch_all_rows($dbh,"select max(grp) from bench1 where region='$region'");
    fetch_all_rows($dbh,"select max(idn) from bench1 where region='$region' and grp=$grp");
    if ($limits->{'group_func_sql_min_str'})
    {
      fetch_all_rows($dbh,"select max(region) from bench1 where region<'$region'");
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,
					   $tests+1, $opt_loop_count));
  }
  print_time($estimated);
  print " for min_max_on_key ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=0;
  $estimated=0;
  for ($tests=0 ; $tests < $opt_loop_count ; $tests++)
  {
    $count+=5;
    $grp=$tests*3 % $opt_groups;
    $region=chr(65+$tests % $opt_regions);
    fetch_all_rows($dbh,"select count(*) from bench1 where region='$region'");
    fetch_all_rows($dbh,"select count(*) from bench1 where region='$region' and grp=$grp");
    fetch_all_rows($dbh,"select count(*) from bench1 where region>'$region'");
    fetch_all_rows($dbh,"select count(*) from bench1 where region<='$region'");
    fetch_all_rows($dbh,"select count(*) from bench1 where region='$region' and grp>$grp");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,
					   $tests+1, $opt_loop_count));
  }
  print_time($estimated);
  print " for count_on_key ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
  
}

if ($limits->{'group_functions'})
{
  $loop_time=new Benchmark;
  $rows=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $rows+=fetch_all_rows($dbh,"select grp,count(*) from bench1 group by grp");
  }
  $end_time=new Benchmark;
  print "Time for count_group_on_key_parts ($i:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}

if ($limits->{'group_distinct_functions'})
{
  print "Testing count(distinct) on the table\n";
  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select count(distinct region) from bench1");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
  }
  print_time($estimated);
  print " for count_distinct_key_prefix ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select count(distinct grp) from bench1");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
  }
  print_time($estimated);
  print " for count_distinct ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

#  Workaround mimer's behavior
  if ($limits->{'multi_distinct'})
  {
    $loop_time=new Benchmark;
    $rows=$estimated=$count=0;
    for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
    {
      $count++;
      $rows+=fetch_all_rows($dbh,"select count(distinct grp),count(distinct rev_idn) from bench1");
      $end_time=new Benchmark;
      last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
    } 
    print_time($estimated);
    print " for count_distinct_2 ($count:$rows): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";
  }

  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select region,count(distinct idn) from bench1 group by region");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
  }
  print_time($estimated);
  print " for count_distinct_group_on_key ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select grp,count(distinct idn) from bench1 group by grp");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
  }
  print_time($estimated);
  print " for count_distinct_group_on_key_parts ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select grp,count(distinct rev_idn) from bench1 group by grp");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $opt_medium_loop_count));
  }
  print_time($estimated);
  print " for count_distinct_group ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $rows=$estimated=$count=0;
  $test_count=$opt_medium_loop_count/10;
  for ($i=0 ; $i < $test_count ; $i++)
  {
    $count++;
    $rows+=fetch_all_rows($dbh,"select idn,count(distinct region) from bench1 group by idn");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i+1,
					   $test_count));
  }
  print_time($estimated);
  print " for count_distinct_big ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}

####
#### End of benchmark
####

if ($opt_lock_tables)
{
  do_query($dbh,"UNLOCK TABLES");
}
if (!$opt_skip_delete)
{
  do_query($dbh,"drop table bench1" . $server->{'drop_attr'});
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

$dbh->disconnect;				# close connection

end_benchmark($start_time);
