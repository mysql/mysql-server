#!/usr/bin/perl -w

# Copyright (c) 2001, 2006 MySQL AB
# Use is subject to license terms
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

#
# This is a test with uses many processes to test a MySQL server.
#
# Tested a lot with:  --threads=30

$opt_loop_count=500000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use DBI;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
$opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_threads=5;
$opt_host=$opt_user=$opt_password=""; $opt_db="test";

GetOptions("host=s","db=s","user=s","password=s","loop-count=i","skip-create","skip-in","skip-delete","verbose","fast-insert","lock-tables","debug","fast","force","threads=i") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$opt_force=undef;  # Ignore warnings from these

print "Test of multiple connections that test the following things:\n";
print "insert, select, delete, update, alter, check, repair and flush\n";

@testtables = ( ["bench_f31", ""],
		["bench_f32", "row_format=fixed"],
		["bench_f33", "delay_key_write=1"],
		["bench_f34", "checksum=1"],
		["bench_f35", "delay_key_write=1"]);
$abort_table="bench_f39";

$numtables = $#testtables+1;
srand 100;			# Make random numbers repeatable

####
####  Start timeing and start test
####

$start_time=new Benchmark;
$dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		    $opt_user, $opt_password,
		  { PrintError => 0}) || die $DBI::errstr;
if (!$opt_skip_create)
{
  my $table_def;
  foreach $table_def (@testtables)
  {
    my ($table,$extra)= ($table_def->[0], $table_def->[1]);
    print "Creating table $table in database $opt_db\n";
    $dbh->do("drop table if exists $table");
    $dbh->do("create table $table".
	     " (id int(6) not null auto_increment,".
	     " info varchar(32)," .
	     " marker timestamp," .
	     " flag int not null," .
	     " primary key(id)) $extra")

      or die $DBI::errstr;
    # One row in the table will make future tests easier
    $dbh->do("insert into $table (id) values (null)")
      or die $DBI::errstr;
  }
  # Create the table we use to signal that we should end the test
  $dbh->do("drop table if exists $abort_table");
  $dbh->do("create table $abort_table (id int(6) not null) ENGINE=heap") ||
    die $DBI::errstr;
}

$dbh->do("delete from $abort_table");
$dbh->disconnect; $dbh=0;	# Close handler
$|= 1;				# Autoflush

####
#### Start the tests
####

for ($i=0 ; $i < $opt_threads ; $i ++)
{
  test_insert() if (($pid=fork()) == 0); $work{$pid}="insert";
}
for ($i=0 ; $i < $numtables ; $i ++)
{
  test_insert($i,$i) if (($pid=fork()) == 0); $work{$pid}="insert_one";
}
for ($i=0 ; $i < $opt_threads ; $i ++)
{
  test_select() if (($pid=fork()) == 0); $work{$pid}="select_key";
}
test_join() if (($pid=fork()) == 0); $work{$pid}="test_join";
test_select_count() if (($pid=fork()) == 0); $work{$pid}="select_count";
test_delete() if (($pid=fork()) == 0); $work{$pid}="delete";
test_update() if (($pid=fork()) == 0); $work{$pid}="update";
test_flush() if (($pid=fork()) == 0); $work{$pid}= "flush";
test_check() if (($pid=fork()) == 0); $work{$pid}="check";
test_repair() if (($pid=fork()) == 0); $work{$pid}="repair";
test_alter() if (($pid=fork()) == 0); $work{$pid}="alter";
#test_database("test2") if (($pid=fork()) == 0); $work{$pid}="check_database";

print "Started " . ($opt_threads*2+4) . " threads\n";

$errors=0;
$running_insert_threads=$opt_threads+$numtables;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finished with exit code $ret\n";
  if ($work{$pid} =~ /^insert/)
  {
    if (!--$running_insert_threads)
    {
      # Time to stop other threads
      signal_abort();
    }
  }
  $errors++ if ($ret != 0);
}

#
# Cleanup
#

if (!$opt_skip_delete && !$errors)
{
  my $table_def;
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $dbh->do("drop table $abort_table");
  foreach $table_def (@testtables)
  {
    $dbh->do("drop table " . $table_def->[0]);
  }
  $dbh->disconnect; $dbh=0;	# Close handler
}

print ($errors ? "Test failed\n" :"Test ok\n");
$end_time=new Benchmark;
print "Total time: " .
  timestr(timediff($end_time, $start_time),"noc") . "\n";

exit(0);


#
# Insert records in the table
#

sub test_insert
{
  my ($from_table,$to_table)= @_;
  my ($dbh,$i,$j,$count,$table_def,$table);

  if (!defined($from_table))
  {
    $from_table=0; $to_table=$numtables-1;
  }

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($i=$count=0 ; $i < $opt_loop_count; $i++)
  {
    for ($j= $from_table ; $j <= $to_table ; $j++)
    {
      my ($table)= ($testtables[$j]->[0]);
      $dbh->do("insert into $table values (NULL,'This is entry $i','',0)") || die "Got error on insert: $DBI::errstr\n";
      $count++;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_insert: Inserted $count rows\n";
  exit(0);
}


#
# select records
# Do continously select over all tables as long as there is changed
# rows in the table
#

sub test_select
{
  my ($dbh, $i, $j, $count, $loop);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $count_query=make_count_query($numtables);
  $count=0;
  $loop=9999;

  $i=0;
  while (($i++ % 100) || !test_if_abort($dbh))
  {
    if ($loop++ >= 100)
    {
      $loop=0;
      $row_counts=simple_query($dbh, $count_query);
    }
    for ($j=0 ; $j < $numtables ; $j++)
    {
      my ($id)= int rand $row_counts->[$j];
      my ($table)= $testtables[$j]->[0];
      simple_query($dbh, "select id,info from $table where id=$id");
      $count++;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_select: Executed $count selects\n";
  exit(0);
}

#
# Do big select count(distinct..) over the table
# 

sub test_select_count
{
  my ($dbh, $i, $j, $count, $loop);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $count=0;
  $i=0;
  while (!test_if_abort($dbh))
  {
    for ($j=0 ; $j < $numtables ; $j++)
    {
      my ($table)= $testtables[$j]->[0];
      simple_query($dbh, "select count(distinct marker),count(distinct id),count(distinct info) from $table");
      $count++;
    }
    sleep(20);		# This query is quite slow
  }
  $dbh->disconnect; $dbh=0;
  print "Test_select: Executed $count select count(distinct) queries\n";
  exit(0);
}

#
# select records
# Do continously joins between the first and second table
#

sub test_join
{
  my ($dbh, $i, $j, $count, $loop);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $count_query=make_count_query($numtables);
  $count=0;
  $loop=9999;

  $i=0;
  while (($i++ % 100) || !test_if_abort($dbh))
  {
    if ($loop++ >= 100)
    {
      $loop=0;
      $row_counts=simple_query($dbh, $count_query);
    }
    for ($j=0 ; $j < $numtables-1 ; $j++)
    {
      my ($id)= int rand $row_counts->[$j];
      my ($t1,$t2)= ($testtables[$j]->[0],$testtables[$j+1]->[0]);
      simple_query($dbh, "select $t1.id,$t2.info from $t1, $t2 where $t1.id=$t2.id and $t1.id=$id");
      $count++;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_join: Executed $count joins\n";
  exit(0);
}

#
# Delete 1-5 rows from the first 2 tables.
# Test ends when the number of rows for table 3 didn't change during
# one loop
#

sub test_delete
{
  my ($dbh, $i,$j, $row_counts, $count_query, $table_count, $count);

  $table_count=2;
  $count=0;
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $count_query=make_count_query($table_count+1);

  sleep(5);			# Give time to insert some rows
  $i=0;
  while (($i++ % 10) || !test_if_abort($dbh))
  {
    sleep(1);
    $row_counts=simple_query($dbh, $count_query);

    for ($j=0 ; $j < $table_count ; $j++)
    {
      my ($id)= int rand $row_counts->[$j];
      my ($table)= $testtables[$j]->[0];
      $dbh->do("delete from $table where id >= $id-2 and id <= $id +2") || die "Got error on delete from $table: $DBI::errstr\n";
      $count++;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_delete: Executed $count deletes\n";
  exit(0);
}

#
# Update the flag for table 2 and 3
# Will abort after a while when table1 doesn't change max value
#

sub test_update
{
  my ($dbh, $i, $j, $row_counts, $count_query, $count, $loop);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $count_query=make_count_query(3);
  $loop=9999;
  $count=0;

  sleep(5);			# Give time to insert some rows
  $i=0;
  while (($i++ % 100) || !test_if_abort($dbh))
  {
    if ($loop++ >= 100)
    {
      $loop=0;
      $row_counts=simple_query($dbh, $count_query);
    }

    for ($j=1 ; $j <= 2 ; $j++)
    {
      my ($id)= int rand $row_counts->[$j];
      my ($table)= $testtables[$j]->[0];
      # Fix to not change the same rows as the above delete
      $id= ($id + $count) % $row_counts->[$j];

      $dbh->do("update $table set flag=flag+1 where id >= $id-2 and id <= $id +2") || die "Got error on update of $table: $DBI::errstr\n";
      $count++;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_update: Executed $count updates\n";
  exit(0);
}


#
# Run a check on all tables except the last one
# (The last one is not checked to put pressure on the key cache)
#

sub test_check
{
  my ($dbh, $row, $i, $j, $type, $table);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $type= "check";
  for ($i=$j=0 ; !test_if_abort($dbh) ; $i++)
  {
    sleep(1000);
    $table=$testtables[$j]->[0];
    $sth=$dbh->prepare("$type table $table") || die "Got error on prepare: $DBI::errstr\n";
    $sth->execute || die $DBI::errstr;

    while (($row=$sth->fetchrow_arrayref))
    {
      if ($row->[3] ne "OK")
      {
	print "Got error " . $row->[3] . " when doing $type on $table\n";
	exit(1);
      }
    }
    if (++$j == $numtables-1)
    {
      $j=0;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "test_check: Executed $i checks\n";
  exit(0);
}

#
# Do a repair on the first table once in a while
#

sub test_repair
{
  my ($dbh, $row, $i, $type, $table);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $type= "repair";
  for ($i=0 ; !test_if_abort($dbh) ; $i++)
  {
    sleep(2000);
    $table=$testtables[0]->[0];
    $sth=$dbh->prepare("$type table $table") || die "Got error on prepare: $DBI::errstr\n";
    $sth->execute || die $DBI::errstr;

    while (($row=$sth->fetchrow_arrayref))
    {
      if ($row->[3] ne "OK")
      {
	print "Got error " . $row->[3] . " when doing $type on $table\n";
	exit(1);
      }
    }
  }
  $dbh->disconnect; $dbh=0;
  print "test_repair: Executed $i repairs\n";
  exit(0);
}

#
# Do a flush tables on table 3 and 4 once in a while
#

sub test_flush
{
  my ($dbh,$count,$tables);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $tables=$testtables[2]->[0] . "," . $testtables[3]->[0];

  $count=0;
  while (!test_if_abort($dbh))
  {
    sleep(3000);
    $dbh->do("flush tables $tables") ||
      die "Got error on flush $DBI::errstr\n";
    $count++;
  }
  $dbh->disconnect; $dbh=0;
  print "flush: Executed $count flushs\n";
  exit(0);
}


#
# Test all tables in a database
#

sub test_database
{
  my ($database) = @_;
  my ($dbh, $row, $i, $type, $tables);
  $dbh = DBI->connect("DBI:mysql:$database:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $tables= join(',',$dbh->func('_ListTables'));
  $type= "check";
  for ($i=0 ; !test_if_abort($dbh) ; $i++)
  {
    sleep(120);
    $sth=$dbh->prepare("$type table $tables") || die "Got error on prepare: $DBI::errstr\n";
    $sth->execute || die $DBI::errstr;

    while (($row=$sth->fetchrow_arrayref))
    {
      if ($row->[3] ne "OK")
      {
	print "Got error " . $row->[2] . " " . $row->[3] . " when doing $type on " . $row->[0] . "\n";
	exit(1);
      }
    }
  }
  $dbh->disconnect; $dbh=0;
  print "test_check: Executed $i checks\n";
  exit(0);
}

#
# Test ALTER TABLE on the second table
#

sub test_alter
{
  my ($dbh, $row, $i, $type, $table);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($i=0 ; !test_if_abort($dbh) ; $i++)
  {
    sleep(100);
    $table=$testtables[1]->[0];
    $sth=$dbh->prepare("ALTER table $table modify info char(32)") || die "Got error on prepare: $DBI::errstr\n";
    $sth->execute || die $DBI::errstr;
  }
  $dbh->disconnect; $dbh=0;
  print "test_alter: Executed $i ALTER TABLE\n";
  exit(0);
}


#
# Help functions
#

sub signal_abort
{
  my ($dbh);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $dbh->do("insert into $abort_table values(1)") || die $DBI::errstr;
  $dbh->disconnect; $dbh=0;
  exit(0);
}


sub test_if_abort()
{
  my ($dbh)=@_;
  $row=simple_query($dbh,"select * from $opt_db.$abort_table");
  return (defined($row) && defined($row->[0]) != 0) ? 1 : 0;
}


sub make_count_query
{
  my ($table_count)= @_;
  my ($tables, $count_query, $i, $tables_def);
  $tables="";
  $count_query="select high_priority ";
  $table_count--;
  for ($i=0 ; $i < $table_count ; $i++)
  {
    my ($table_def)= $testtables[$i];
    $tables.=$table_def->[0] . ",";
    $count_query.= "max(" . $table_def->[0] . ".id),";
  }
  $table_def=$testtables[$table_count];
  $tables.=$table_def->[0];
  $count_query.= "max(" . $table_def->[0] . ".id) from $tables";
  return $count_query;
}

sub simple_query()
{
  my ($dbh, $query)= @_;
  my ($sth,$row);

  $sth=$dbh->prepare($query) || die "Got error on '$query': " . $dbh->errstr . "\n";
  $sth->execute || die "Got error on '$query': " . $dbh->errstr . "\n";
  $row= $sth->fetchrow_arrayref();
  $sth=0;
  return $row;
}
