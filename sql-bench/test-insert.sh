#@PERL@
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
# Test of creating a simple table and inserting $record_count records in it,
# $opt_loop_count rows in order, $opt_loop_count rows in reverse order and
# $opt_loop_count rows in random order
#
# changes made for Oracle compatibility
# - $limits{'func_odbc_mod'} is OK from crash-me, but it fails here so set we
#   set it to 0 in server-cfg
# - the default server config runs out of rollback segments, so I added a couple
#   of disconnect/connects to reset
##################### Standard benchmark inits ##############################

use DBI;
use Benchmark;

$opt_loop_count=100000;		# number of rows/3
$small_loop_count=10;		# Loop for full table retrieval
$range_loop_count=$small_loop_count*50;
$many_keys_loop_count=$opt_loop_count;

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

if ($opt_loop_count < 256)
{
  $opt_loop_count=256;		# Some tests must have some data to work!
}

if ($opt_small_test)
{
  $opt_loop_count/=100;
  $range_loop_count/=10;
  $many_keys_loop_count=$opt_loop_count/10;
}
elsif ($opt_small_tables)
{
  $opt_loop_count=10000;		# number of rows/3
  $many_keys_loop_count=$opt_loop_count;
}
elsif ($opt_small_key_tables)
{
  $many_keys_loop_count/=10;
}

print "Testing the speed of inserting data into 1 table and do some selects on it.\n";
print "The tests are done with a table that has $opt_loop_count rows.\n\n";

####
#### Generating random keys
####

print "Generating random keys\n";
$random[$opt_loop_count]=0;
for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $random[$i]=$i+$opt_loop_count;
}

my $tmpvar=1;
for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $tmpvar^= ((($tmpvar + 63) + $i)*3 % $opt_loop_count);
  $swap=$tmpvar % $opt_loop_count;
  $tmp=$random[$i]; $random[$i]=$random[$swap]; $random[$swap]=$tmp;
}

$total_rows=$opt_loop_count*3;

####
####  Connect and start timeing
####
$start_time=new Benchmark;
$dbh = $server->connect();
####
#### Create needed tables
####

goto keys_test if ($opt_stage == 2);
goto select_test if ($opt_skip_create);

print "Creating tables\n";
$dbh->do("drop table bench1");
do_many($dbh,$server->create("bench1",
			     ["id int NOT NULL",
			      "id2 int NOT NULL",
			      "id3 int NOT NULL",
			      "dummy1 char(30)"],
			     ["primary key (id,id2)",
			     "index index_id3 (id3)"]));

if ($opt_lock_tables)
{
  $sth = $dbh->do("LOCK TABLES bench1 WRITE") || die $DBI::errstr;
}

####
#### Insert $total_rows records in order, in reverse order and random.
####

$loop_time=new Benchmark;

if ($opt_fast_insert)
{
  $query="insert into bench1 values ";
}
else
{
  $query="insert into bench1 (id,id2,id3,dummy1) values ";
}

if (($opt_fast || $opt_fast_insert) && $limits->{'multi_value_insert'})
{
  $query_size=$server->{'limits'}->{'query_size'};

  print "Inserting $opt_loop_count multiple-value rows in order\n";
  $res=$query;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmp= "($i,$i,$i,'ABCDEFGHIJ'),";
    if (length($tmp)+length($res) < $query_size)
    {
      $res.= $tmp;
    }
    else
    {
      $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
      $res=$query . $tmp;
    }
  }
  print "Inserting $opt_loop_count multiple-value rows in reverse order\n";
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmp= "(" . ($total_rows-1-$i) . "," .($total_rows-1-$i) .
      "," .($total_rows-1-$i) . ",'BCDEFGHIJK'),";
    if (length($tmp)+length($res) < $query_size)
    {
      $res.= $tmp;
    }
    else
    {
      $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
      $res=$query . $tmp;
    }
  }
  print "Inserting $opt_loop_count multiple-value rows in random order\n";
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmp= "(" . $random[$i] . "," . $random[$i] . "," . $random[$i] .
      ",'CDEFGHIJKL')," or die $DBI::errstr;
    if (length($tmp)+length($res) < $query_size)
    {
      $res.= $tmp;
    }
    else
    {
      $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
      $res=$query . $tmp;
    }
  }
  $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
}
else
{
  print "Inserting $opt_loop_count rows in order\n";
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth = $dbh->do($query . "($i,$i,$i,'ABCDEFGHIJ')") or die $DBI::errstr;
  }

  print "Inserting $opt_loop_count rows in reverse order\n";
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth = $dbh->do($query . "(" . ($total_rows-1-$i) . "," .
		    ($total_rows-1-$i) . "," .
		    ($total_rows-1-$i) . ",'BCDEFGHIJK')")
      or die $DBI::errstr;
  }

  print "Inserting $opt_loop_count rows in random order\n";

  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth = $dbh->do($query . "(". $random[$i] . "," . $random[$i] .
		    "," . $random[$i] . ",'CDEFGHIJKL')") or die $DBI::errstr;
  }
}

$end_time=new Benchmark;
print "Time for insert (" . ($total_rows) . "): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

####
#### insert $opt_loop_count records with duplicate id
####

print "Testing insert of duplicates\n";
$loop_time=new Benchmark;
for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $tmpvar^= ((($tmpvar + 63) + $i)*3 % $opt_loop_count);
  $tmp=$tmpvar % ($total_rows);
  $tmpquery = "$query" . "$tmp" . ",1,2,'D')";
  if ($dbh->do($tmpquery))
  {
    die "Didn't get an error when inserting duplicate record $tmp\n";
  }
}

$end_time=new Benchmark;
print "Time for insert_duplicates (" . ($total_rows) . "): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

####
#### Do some selects on the table
####

select_test:

print "Retrieving data from the table\n";
$loop_time=new Benchmark;
$error=0;

# It's really a small table, so we can try a select on everything

$count=0;
for ($i=1 ; $i <= $small_loop_count ; $i++)
{
  if (($found_rows=fetch_all_rows($dbh,"select id from bench1")) !=
      $total_rows)
  {
    if (!$error++)
    {
      print "Warning: Got $found_rows rows when selecting a hole table of " . ($total_rows) . " rows\nContact the database or DBD author!\n";
    }
  }
  $count+=$found_rows;
}

$end_time=new Benchmark;
print "Time for select_big ($small_loop_count:$count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

$loop_time=new Benchmark;
$estimated=0;
$rows=0;
$count=0;
for ($i=1 ; $i <= $small_loop_count/2 ; $i++)
{
  $rows+=fetch_all_rows($dbh,"select id from bench1 order by id",1);
  $rows+=fetch_all_rows($dbh,"select id from bench1 order by id desc",1);
  $count+=2;
  $end_time=new Benchmark;
  last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$count,
					 $small_loop_count));
}
if ($estimated)
{ print "Estimated time"; }
else
{ print "Time"; }
print " for order_by_key ($count:$rows): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";

$loop_time=new Benchmark;
$estimated=0;
$rows=0;
$count=0;
for ($i=1 ; $i <= $small_loop_count/2 ; $i++)
{
  $rows+=fetch_all_rows($dbh,"select id2 from bench1 order by id2",1);
  $rows+=fetch_all_rows($dbh,"select id2 from bench1 order by id2 desc",1);
  $count+=2;
  $end_time=new Benchmark;
  last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$count,
					 $small_loop_count));
}
if ($estimated)
{ print "Estimated time"; }
else
{ print "Time"; }
print " for order_by ($count:$rows): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";

#
# Test of select on 2 different keys with or
# (In this case database can only use keys if they do an automatic union).
#

$loop_time=new Benchmark;
$estimated=0;
$rows=0;
$count=0;
for ($i=1 ; $i <= $range_loop_count ; $i++)
{
  my $rnd=$i;
  my $rnd2=$random[$i];
  $rows+=fetch_all_rows($dbh,"select id2 from bench1 where id=$rnd or id3=$rnd2",1);
  $count++;
  $end_time=new Benchmark;
  last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$count,
					 $range_loop_count));
}
if ($estimated)
{ print "Estimated time"; }
else
{ print "Time"; }
print " for select_diff_key ($count:$rows): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";


# Test select that is very popular when using ODBC

check_or_range("id","select_range_prefix");
check_or_range("id3","select_range");

# Check reading on direct key on id and id3

check_select_key("id","select_key_prefix");
check_select_key("id3","select_key");

####
#### A lot of simple selects on ranges
####

@Q=("select * from bench1 where !id!=3 or !id!=2 or !id!=1 or !id!=4 or !id!=16 or !id!=10",
    6,
    "select * from bench1 where !id!>=" . ($total_rows-1) ." or !id!<1",
    2,
    "select * from bench1 where !id!>=1 and !id!<=2",
    2,
    "select * from bench1 where (!id!>=1 and !id!<=2) or (!id!>=1 and !id!<=2)",
    2,
    "select * from bench1 where !id!>=1 and !id!<=10 and !id!<=5",
    5,
    "select * from bench1 where (!id!>0 and !id!<2) or !id!>=" . ($total_rows-1),
    2,
    "select * from bench1 where (!id!>0 and !id!<2) or (!id!>= " . ($opt_loop_count/2) . " and !id! <= " . ($opt_loop_count/2+2) . ") or !id! = " . ($opt_loop_count/2-1),
    5,
    "select * from bench1 where (!id!>=5 and !id!<=10) or (!id!>=1 and !id!<=4)",
    10,
    "select * from bench1 where (!id!=1 or !id!=2) and (!id!=3 or !id!=4)",
    0,
    "select * from bench1 where (!id!=1 or !id!=2) and (!id!=2 or !id!=3)",
    1,
    "select * from bench1 where (!id!=1 or !id!=5 or !id!=20 or !id!=40) and (!id!=1 or !id!>=20 or !id!=4)",
    3,
    "select * from bench1 where ((!id!=1 or !id!=3) or (!id!>1 and !id!<3)) and !id!<=2",
    2,
    "select * from bench1 where (!id! >= 0 and !id! < 4) or (!id! >=4 and !id! < 6)",
    6,
    "select * from bench1 where !id! <= -1 or (!id! >= 0 and !id! <= 5) or (!id! >=4 and !id! < 6) or (!id! >=6 and !id! <=7) or (!id!>7 and !id! <= 8)",
    9,
    "select * from bench1 where (!id!>=1 and !id!<=2 or !id!>=4 and !id!<=5) or (!id!>=0 and !id! <=10)",
    11,
    "select * from bench1 where (!id!>=1 and !id!<=2 or !id!>=4 and !id!<=5) or (!id!>2 and !id! <=10)",
    10,
    "select * from bench1 where (!id!>1 or !id! <1) and !id!<=2",
    2,
    "select * from bench1 where !id! <= 2 and (!id!>1 or !id! <=1)",
    3,
    "select * from bench1 where (!id!>=1 or !id! <1) and !id!<=2",
    3,
    "select * from bench1 where (!id!>=1 or !id! <=2) and !id!<=2",
    3
    );

print "\nTest of compares with simple ranges\n";
check_select_range("id","select_range_prefix");
check_select_range("id3","select_range");

####
#### Some group queries
####

if ($limits->{'group_functions'})
{
  $loop_time=new Benchmark;
  $count=1;

  for ($tests=0 ; $tests < $small_loop_count ; $tests++)
  {
    $sth=$dbh->prepare($query="select count(*) from bench1") or die $DBI::errstr;
    $sth->execute or die $sth->errstr;
    if (($sth->fetchrow_array)[0] != $total_rows)
    {
      print "Warning: '$query' returned wrong result\n";
    }
    $sth->finish;

    # min, max in keys are very normal
    $count+=7;
    fetch_all_rows($dbh,"select min(id) from bench1");
    fetch_all_rows($dbh,"select max(id) from bench1");
    fetch_all_rows($dbh,"select sum(id+0.0) from bench1");
    fetch_all_rows($dbh,"select min(id3),max(id3),sum(id3 +0.0) from bench1");
    if ($limits->{'group_func_sql_min_str'})
    {
      fetch_all_rows($dbh,"select min(dummy1),max(dummy1) from bench1");
    }
    $count++;
    $sth=$dbh->prepare($query="select count(*) from bench1 where id >= " .
		       ($opt_loop_count*2)) or die $DBI::errstr;
    $sth->execute or die $DBI::errstr;
    if (($sth->fetchrow_array)[0] != $opt_loop_count)
    {
      print "Warning: '$query' returned wrong result\n";
    }
    $sth->finish;


    $count++;
    $sth=$dbh->prepare($query="select count(*),sum(id+0.0),min(id),max(id),avg(id+0.0) from bench1") or die $DBI::errstr;
    $sth->execute or die $DBI::errstr;
    @row=$sth->fetchrow_array;
    if ($row[0] != $total_rows ||
	int($row[1]+0.5) != int((($total_rows-1)/2*$total_rows)+0.5) ||
	$row[2] != 0 ||
	$row[3] != $total_rows-1 ||
	1-$row[4]/(($total_rows-1)/2) > 0.001)
    {
      # PostgreSQL 6.3 fails here
      print "Warning: '$query' returned wrong result: @row\n";
    }
    $sth->finish;

    if ($limits->{'func_odbc_mod'})
    {
      $tmp="mod(id,10)";
      if ($limits->{'func_extra_%'})
      {
	$tmp="id % 10";		# For postgreSQL
      }
      $count++;
      if ($limits->{'group_by_alias'}) {
	if (fetch_all_rows($dbh,$query=$server->query("select $tmp as last_digit,count(*) from bench1 group by last_digit")) != 10)
	{
	  print "Warning: '$query' returned wrong number of rows\n";
	}
      } elsif ($limits->{'group_by_position'}) {
	if (fetch_all_rows($dbh,$query=$server->query("select $tmp,count(*) from bench1 group by 1")) != 10)
	{
	  print "Warning: '$query' returned wrong number of rows\n";
	}
      }
    }

    if ($limits->{'order_by_position'} && $limits->{'group_by_position'})
    {
      $count++;
      if (fetch_all_rows($dbh, $query="select id,id3,dummy1 from bench1 where id < 100+$count-$count group by id,id3,dummy1 order by id desc,id3,dummy1") != 100)
      {
	print "Warning: '$query' returned wrong number of rows\n";
      }
    }
  }
  $end_time=new Benchmark;
  print "Time for select_group ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=$estimated=0;
  for ($tests=1 ; $tests <= $range_loop_count*5 ; $tests++)
  {
    $count+=6;
    fetch_all_rows($dbh,"select min(id) from bench1");
    fetch_all_rows($dbh,"select max(id) from bench1");
    fetch_all_rows($dbh,"select min(id2) from bench1 where id=$tests");
    fetch_all_rows($dbh,"select max(id2) from bench1 where id=$tests");
    if ($limits->{'group_func_sql_min_str'})
    {
      fetch_all_rows($dbh,"select min(dummy1) from bench1 where id=$tests");
      fetch_all_rows($dbh,"select max(dummy1) from bench1 where id=$tests");
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$tests,
					   $range_loop_count*5));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for min_max_on_key ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=$estimated=0;
  for ($tests=1 ; $tests <= $small_loop_count ; $tests++)
  {
    $count+=6;
    fetch_all_rows($dbh,"select min(id2) from bench1");
    fetch_all_rows($dbh,"select max(id2) from bench1");
    fetch_all_rows($dbh,"select min(id3) from bench1 where id2=$tests");
    fetch_all_rows($dbh,"select max(id3) from bench1 where id2=$tests");
    if ($limits->{'group_func_sql_min_str'})
    {
      fetch_all_rows($dbh,"select min(dummy1) from bench1 where id2=$tests");
      fetch_all_rows($dbh,"select max(dummy1) from bench1 where id2=$tests");
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$tests,
					   $range_loop_count));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for min_max ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=0;
  $total=$opt_loop_count*3;
  for ($tests=0 ; $tests < $total ; $tests+=$total/100)
  {
    $count+=1;
    fetch_all_rows($dbh,"select count(id) from bench1 where id < $tests");
  }
  $end_time=new Benchmark;
  print "Time for count_on_key ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=0;
  for ($tests=0 ; $tests < $total ; $tests+=$total/100)
  {
    $count+=1;
    fetch_all_rows($dbh,"select count(dummy1) from bench1 where id2 < $tests");
  }
  $end_time=new Benchmark;
  print "Time for count ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=$estimated=0;
  for ($tests=0 ; $tests < $small_loop_count ; $tests++)
  {
    $count+=2;
    fetch_all_rows($dbh,"select count(distinct dummy1) from bench1");
    fetch_all_rows($dbh,"select dummy1,count(distinct id) from bench1 group by dummy1");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$tests,
					   $small_loop_count));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for count_distinct_big ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";
}


if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}

####
#### Some updates on the table
####

$loop_time=new Benchmark;

if ($limits->{'functions'})
{
  print "\nTesting update of keys with functions\n";
  my $update_loop_count=$opt_loop_count/2;
  for ($i=0 ; $i < $update_loop_count ; $i++)
  {
    my $tmp=$opt_loop_count+$random[$i]; # $opt_loop_count*2 <= $tmp < $total_rows
    $sth = $dbh->do("update bench1 set id3=-$tmp where id3=$tmp") or die $DBI::errstr;
  }

  $end_time=new Benchmark;
  print "Time for update_of_key ($range_loop_count):  " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  if ($opt_fast && defined($server->{vacuum}))
  {
    $server->vacuum(1,\$dbh);
  }

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }

  $loop_time=new Benchmark;
  $count=0;
  $step=int($opt_loop_count/$range_loop_count+1);
  for ($i= 0 ; $i < $opt_loop_count ; $i+= $step)
  {
    $count++;
    $sth=$dbh->do("update bench1 set id3= 0-id3 where id3 >= 0 and id3 <= $i") or die $DBI::errstr;
  }

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  $count++;
  $sth=$dbh->do("update bench1 set id3= 0-id3 where id3 >= 0 and id3 < $opt_loop_count") or die $DBI::errstr;

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  $count++;
  $sth=$dbh->do("update bench1 set id3= 0-id3 where id3 >= $opt_loop_count and id3 < ". ($opt_loop_count*2)) or die $DBI::errstr;

  #
  # Check that everything was updated
  # In principle we shouldn't time this in the update loop..
  #

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  $row_count=0;
  if (($sth=$dbh->prepare("select count(*) from bench1 where id3>=0"))
      && $sth->execute)
  {
    ($row_count)=$sth->fetchrow;
  }
  $result=1 + $opt_loop_count-$update_loop_count;
  if ($row_count != $result)
  {
    print "Warning: Update check returned $row_count instead of $result\n";
  }

  $sth->finish;
  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  #restore id3 to 0 <= id3 < $total_rows/10 or 0<= id3 < $total_rows

  my $func=($limits->{'func_odbc_floor'}) ? "floor((0-id3)/20)" : "0-id3";
  $count++;
  $sth=$dbh->do($query="update bench1 set id3=$func where id3<0") or die $DBI::errstr;

  $end_time=new Benchmark;
  print "Time for update_of_key_big ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}
else
{
  print "\nTesting update of keys in loops\n";
  #
  # This is for mSQL that doesn't have functions. Do we really need this ????
  #

  $sth=$dbh->prepare("select id3 from bench1 where id3 >= 0") or die $DBI::errstr;
  $sth->execute or die $DBI::errstr;
  $count=0;
  while (@tmp = $sth->fetchrow_array)
  {
    my $tmp1 = "-$tmp[0]";
    my $sth1 = $dbh->do("update bench1 set id3 = $tmp1 where id3 = $tmp[0]");
    $count++;
    $end_time=new Benchmark;
    if (($end_time->[0] - $loop_time->[0]) > $opt_time_limit)
    {
      print "note: Aborting update loop because of timeout\n";
      last;
    }
  }
  $sth->finish;
  # Check that everything except id3=0 was updated
  # In principle we shouldn't time this in the update loop..
  #
  if (fetch_all_rows($dbh,$query="select * from bench1 where id3>=0") != 1)
  {
    if ($count == $total_rows)
    {
      print "Warning: Wrong information after update: Found '$row_count' rows, but should have been: 1\n";
    }
  }
  #restore id3 to 0 <= id3 < $total_rows
  $sth=$dbh->prepare("select id3 from bench1 where id3 < 0") or die $DBI::errstr;
  $sth->execute or die $DBI::errstr;
  while (@tmp = $sth->fetchrow_array)
  {
    $count++;
    my $tmp1 = floor((0-$tmp[0])/10);
    my $sth1 = $dbh->do("update bench1 set id3 = $tmp1 where id3 = $tmp[0]");
  }
  $sth->finish;
  $end_time=new Benchmark;
  $estimated=predict_query_time($loop_time,$end_time,\$count,$count,
				$opt_loop_count*6);
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for update_of_key ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#
# Testing some simple updates
#

print "Testing update with key\n";
$loop_time=new Benchmark;
for ($i=0 ; $i < $opt_loop_count*3 ; $i++)
{
  $sth = $dbh->do("update bench1 set dummy1='updated' where id=$i") or die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for update_with_key ($opt_loop_count):  " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";

print "\nTesting update of all rows\n";
$loop_time=new Benchmark;
for ($i=0 ; $i < $small_loop_count ; $i++)
{
  $sth = $dbh->do("update bench1 set dummy1='updated $i'") or die $DBI::errstr;
}
$end_time=new Benchmark;
print "Time for update_big ($range_loop_count):  " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";


#
# Testing left outer join
#

if ($limits->{'func_odbc_floor'} && $limits->{'left_outer_join'})
{
  if ($opt_lock_tables)
  {
    $sth = $dbh->do("LOCK TABLES bench1 a READ, bench1 b READ") || die $DBI::errstr;
  }
  print "\nTesting left outer join\n";
  $loop_time=new Benchmark;
  $count=0;
  for ($i=0 ; $i < $small_loop_count ; $i++)
  {
    $count+=fetch_all_rows($dbh,"select count(*) from bench1 as a left outer join bench1 as b on (a.id2=b.id3)");
  }
  $end_time=new Benchmark;
  print "Time for outer_join_on_key ($small_loop_count:$count):  " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $loop_time=new Benchmark;
  $count=0;
  for ($i=0 ; $i < $small_loop_count ; $i++)
  {
    $count+=fetch_all_rows($dbh,"select count(a.dummy1),count(b.dummy1) from bench1 as a left outer join bench1 as b on (a.id2=b.id3)");
  }
  $end_time=new Benchmark;
  print "Time for outer_join ($small_loop_count:$count):  " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $count=0;
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $small_loop_count ; $i++)
  {
    $count+=fetch_all_rows($dbh,"select count(a.dummy1),count(b.dummy1) from bench1 as a left outer join bench1 as b on (a.id2=b.id3) where b.id3 is not null");
  }
  $end_time=new Benchmark;
  print "Time for outer_join_found ($small_loop_count:$count):  " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  $count=$estimated=0;
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $small_loop_count ; $i++)
  {
    $count+=fetch_all_rows($dbh,"select count(a.dummy1),count(b.dummy1) from bench1 as a left outer join bench1 as b on (a.id2=b.id3) where b.id3 is null");
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,
					   \$count,$i,
					   $range_loop_count));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for outer_join_not_found ($range_loop_count:$count):  " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  if ($opt_lock_tables)
  {
    $sth = $dbh->do("LOCK TABLES bench1 WRITE") || die $DBI::errstr;
  }
}

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}

####
#### Do some deletes on the table
####

if (!$opt_skip_delete)
{
  print "\nTesting delete\n";
  $loop_time=new Benchmark;
  $count=0;
  for ($i=0 ; $i < $opt_loop_count ; $i+=10)
  {
    $count++;
    $tmp=$opt_loop_count+$random[$i]; # $opt_loop_count*2 <= $tmp < $total_rows
    $dbh->do("delete from bench1 where id3=$tmp") or die $DBI::errstr;
  }

  $end_time=new Benchmark;
  print "Time for delete_key ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }

  $count=0;
  $loop_time=new Benchmark;
  for ($i= 0 ; $i < $opt_loop_count ; $i+=$opt_loop_count/10)
  {
    $sth=$dbh->do("delete from bench1 where id3 >= 0 and id3 <= $i") or die $DBI::errstr;
    $count++;
  }
  $count+=2;
  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  $sth=$dbh->do("delete from bench1 where id3 >= 0 and id3 <= $opt_loop_count") or die $DBI::errstr;
  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }

  $sth=$dbh->do("delete from bench1 where id >= $opt_loop_count and id <= " . ($opt_loop_count*2) ) or die $DBI::errstr;

  if ($server->small_rollback_segment())
  {
    $dbh->disconnect;				# close connection
    $dbh = $server->connect();
  }
  if ($opt_fast)
  {
    $sth=$dbh->do("delete from bench1") or die $DBI::errstr;
  }
  else
  {
    $sth = $dbh->do("delete from bench1 where id3 < " . ($total_rows)) or die $DBI::errstr;
  }

  $end_time=new Benchmark;
  print "Time for delete_big ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

  if ($opt_lock_tables)
  {
    $sth = $dbh->do("UNLOCK TABLES ") || die $DBI::errstr;
  }
  $sth = $dbh->do("drop table bench1") or die $DBI::errstr;
}

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}
if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}


keys_test:
#
# Test of insert in table with many keys
# This test assumes that the server really create the keys!
#

my @fields=(); my @keys=();
$keys=min($limits->{'max_index'},16);		  # 16 is more than enough
$seg= min($limits->{'max_index_parts'},$keys,16); # 16 is more than enough

print "Insert into table with $keys keys and with a primary key with $seg parts\n";

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
  push(@fields,"field$i char(6) not null");	# Should be relatively fair
}

# First key contains many segments
$query="primary key (";
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

do_many($dbh,$server->create("bench1",\@fields,\@keys));
if ($opt_lock_tables)
{
  $dbh->do("LOCK TABLES bench1 WRITE") || die $DBI::errstr;
}

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}

$loop_time=new Benchmark;
$fields=$#fields;
if (($opt_fast || $opt_fast_insert) && $limits->{'multi_value_insert'})
{
  $query_size=$server->{'limits'}->{'query_size'};
  $query="insert into bench1 values ";
  $res=$query;
  for ($i=0; $i < $many_keys_loop_count; $i++)
  {
    $rand=$random[$i];
    $tmp="(" . ($i & 127) . ",$rand," . ($i & 32766) .
      ",'ABCDEF$rand',0,";

    for ($j=5; $j <= $fields ; $j++)
    {
      $tmp.= ($types[$j] == 0) ? "$rand," : "'$rand',";
    }
    substr($tmp,-1)=")";
    if (length($tmp)+length($res) < $query_size)
    {
      $res.= $tmp . ",";
    }
    else
    {
      $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
      $res=$query . $tmp . ",";
    }
  }
  $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
}
else
{
  for ($i=0; $i < $many_keys_loop_count; $i++)
  {
    $rand=$random[$i];
    $query="insert into bench1 values (" . ($i & 127) . ",$rand," . ($i & 32767) .
      ",'ABCDEF$rand',0,";

    for ($j=5; $j <= $fields ; $j++)
    {
      $query.= ($types[$j] == 0) ? "$rand," : "'$rand',";
    }
    substr($query,-1)=")";
    print "query1: $query\n" if ($opt_debug);
    $dbh->do($query) or die "Got error $DBI::errstr with query: $query\n";
  }
}
$end_time=new Benchmark;
print "Time for insert_key ($many_keys_loop_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}
if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#
# update one key of the above
#

print "Testing update of keys\n";
$loop_time=new Benchmark;
for ($i=0 ; $i< 256; $i++)
{
  $dbh->do("update bench1 set field5=1 where field1=$i")
    or die "Got error $DBI::errstr with query: update bench1 set field5=1 where field1=$i\n";
}
$end_time=new Benchmark;
print "Time for update_of_key (256): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}
if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh = $server->connect();
}

#
# Delete everything from table
#

print "Deleting everything from table\n";
$loop_time=new Benchmark;
$count=0;
if ($opt_fast)
{
  $dbh->do("delete from bench1 where field1 = 0") or die $DBI::errstr;
  $dbh->do("delete from bench1") or die $DBI::errstr;
  $count+=2;
}
else
{
  $dbh->do("delete from bench1 where field1 = 0") or die $DBI::errstr;
  $dbh->do("delete from bench1 where field1 > 0") or die $DBI::errstr;
  $count+=2;
}

if ($opt_lock_tables)
{
  $sth = $dbh->do("UNLOCK TABLES") || die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for delete_big_many_keys ($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

$sth = $dbh->do("drop table bench1") or die $DBI::errstr;
if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

#
# Test multi value inserts if the server supports it
#

if ($limits->{'multi_value_insert'})
{
  $query_size=$limits->{'query_size'}; # Same limit for all databases

  $sth = $dbh->do("drop table bench1");
  do_many($dbh,$server->create("bench1",
			       ["id int NOT NULL",
				"id2 int NOT NULL",
				"id3 int NOT NULL",
				"dummy1 char(30)"],
			       ["primary key (id,id2)",
			       "index index_id3 (id3)"]));
  if ($opt_lock_tables)
  {
    $sth = $dbh->do("LOCK TABLES bench1 write") || die $DBI::errstr;
  }

  $loop_time=new Benchmark;
  print "Inserting $opt_loop_count rows with multiple values\n";
  $query="insert into bench1 values ";
  $res=$query;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    my $tmp= "($i,$i,$i,'EFGHIJKLM'),";
    if (length($i)+length($res) < $query_size)
    {
      $res.= $tmp;
    }
    else
    {
      do_query($dbh,substr($res,0,length($res)-1));
      $res=$query .$tmp;
    }
  }
  do_query($dbh,substr($res,0,length($res)-1));

  if ($opt_lock_tables)
  {
    $sth = $dbh->do("UNLOCK TABLES ") || die $DBI::errstr;
  }

  $end_time=new Benchmark;
  print "Time for multiple_value_insert (" . ($opt_loop_count) . "): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

  if ($opt_fast && defined($server->{vacuum}))
  {
    $server->vacuum(1,\$dbh);
  }
  if ($opt_lock_tables)
  {
    $sth = $dbh->do("UNLOCK TABLES ") || die $DBI::errstr;
  }

  # A big table may take a while to drop
  $loop_time=new Benchmark;
  $sth = $dbh->do("drop table bench1") or die $DBI::errstr;
  $end_time=new Benchmark;
  print "Time for drop table(1): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

####
#### End of benchmark
####

$dbh->disconnect;				# close connection

end_benchmark($start_time);

###
### Some help functions
###


# Do some sample selects on direct key
# First select finds a row, the second one doesn't find.

sub check_select_key
{
  my ($column,$check)= @_;
  my ($loop_time,$end_time,$i,$tmp_var,$tmp,$count,$row_count,$estimated);

  $estimated=0;
  $loop_time=new Benchmark;
  $count=0;
  for ($i=1 ; $i <= $opt_loop_count; $i++)
  {
    $count+=2;
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % $opt_loop_count);
    $tmp=$tmpvar % ($total_rows);
    fetch_all_rows($dbh,"select * from bench1 where $column=$tmp")
      or die $DBI::errstr;
    $tmp+=$total_rows;
    defined($row_count=fetch_all_rows($dbh,"select * from bench1 where $column=$tmp")) or die $DBI::errstr;
    die "Found $row_count rows on impossible id: $tmp\n" if ($row_count);
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i,
					   $opt_loop_count));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for $check ($count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}

#
# Search using some very simple queries
#

sub check_select_range
{
  my ($column,$check)= @_;
  my ($loop_time,$end_time,$i,$tmp_var,$tmp,$query,$rows,$estimated);

  $estimated=0;
  $loop_time=new Benchmark;
  $found=$count=0;
  for ($test=1 ; $test <= $range_loop_count; $test++)
  {
    $count+=$#Q+1;
    for ($i=0 ; $i < $#Q ; $i+=2)
    {
      $query=$Q[$i];
      $rows=$Q[$i+1];
      $query =~ s/!id!/$column/g;
      if (($row_count=fetch_all_rows($dbh,$query)) != $rows)
      {
	if ($row_count == undef())
	{
	  die "Got error: $DBI::errstr when executing $query\n";
	}
	die "'$query' returned wrong number of rows: $row_count instead of $rows\n";
      }
      $found+=$row_count;
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$test,
					   $range_loop_count));
  }
  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for $check ($count:$found): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}


#
# SELECT * from bench where col=x or col=x or col=x ...


sub check_or_range
{
  my ($column,$check)= @_;
  my ($loop_time,$end_time,$i,$tmp_var,$tmp,$columns,$estimated,$found,
      $or_part,$count,$loop_count);

  $columns=min($limits->{'max_columns'},50,($limits->{'query_size'}-50)/13);
  $columns=$columns- ($columns % 4); # Make Divisible by 4

  $estimated=0;
  $loop_time=new Benchmark;
  $found=0;
  $loop_count=$range_loop_count*10;
  for ($count=0 ; $count < $loop_count ; )
  {
    for ($rowcnt=0; $rowcnt <= $columns; $rowcnt+= $columns/4)
    {
      my $query="select * from bench1 where ";
      my $or_part= "$column = 1";
      $count+=2;

      for ($i=1 ; $i < $rowcnt ; $i++)
      {
	$tmpvar^= ((($tmpvar + 63) + $i)*3 % $opt_loop_count);
	$tmp=$tmpvar % ($opt_loop_count*4);
	$or_part.=" or $column=$tmp";
      }
      print $query . $or_part . "\n" if ($opt_debug);
      ($rows=fetch_all_rows($dbh,$query . $or_part)) or die $DBI::errstr;
      $found+=$rows;

      if ($limits->{'func_extra_in_num'})
      {
	my $in_part=$or_part;	# Same query, but use 'func_extra_in_num' instead.
	$in_part=~ s/ = / IN \(/;
	$in_part=~ s/ or $column=/,/g;
	$in_part.= ")";
	fetch_all_rows($dbh,$query . $in_part) or die $DBI::errstr;
	$count++;
      }
      # Do it a little harder by setting a extra range
      defined(($rows=fetch_all_rows($dbh,"$query($or_part) and $column < 10"))) or die $DBI::errstr;
      $found+=$rows;
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$count,
					   $loop_count));
  }

  if ($estimated)
  { print "Estimated time"; }
  else
  { print "Time"; }
  print " for $check ($count:$found): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}
