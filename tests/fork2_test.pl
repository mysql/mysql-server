#!/usr/bin/perl -w

# Copyright (C) 2000, 2001 MySQL AB
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

# This is a test with uses 5 processes to insert, update and select from
# two tables.
# One inserts records in the tables, one updates some record in it and
# the last 3 does different selects on the tables.
# Er, hmmm..., something like that :^)
# Modified to do crazy-join, à la Nasdaq.
#
# This test uses the old obsolete mysql interface. For a test that uses
# DBI, please take a look at fork_big.pl

$opt_loop_count=10000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use Mysql;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
  $opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_host=""; $opt_db="test";

GetOptions("host=s","db=s","loop-count=i","skip-create","skip-in",
	   "skip-delete", "verbose","fast-insert","lock-tables","debug","fast",
	   "force") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$Mysql::db_errstr=$opt_force=undef;  # Ignore warnings from these

print "Testing 10 multiple connections to a server with 1 insert/update\n";
print "and 8 select connections and one ALTER TABLE.\n";


@testtables = qw(bench_f21 bench_f22 bench_f23 bench_f24 bench_f25);
$numtables = $#testtables;	# make emacs happier
$dtable = "directory";
####  
####  Start timeing and start test
####

$start_time=new Benchmark;
if (!$opt_skip_create)
{
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $Mysql::QUIET = 1;
  foreach $table (@testtables) {
      $dbh->Query("drop table $table");
  }
  $dbh->Query("drop table $dtable");
  $Mysql::QUIET = 0;

  foreach $table (@testtables) {
      print "Creating table $table in database $opt_db\n";
      $dbh->Query("create table $table".
		  " (id int(6) not null,".
		  " info varchar(32),".
		  " marker timestamp,".
		  " primary key(id))")
	  or die $Mysql::db_errstr;
  }
  print "Creating directory table $dtable in $opt_db\n";
  $dbh->Query("create table $dtable (id int(6), last int(6))")
      or die $Mysql::db_errstr;
  # Populate directory table
  for $i ( 0 .. $numtables ) {
      $dbh->Query("insert into $dtable values($i, 0)");
  }
  $dbh=0;			# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

#$test_index = 0;

test_1() if (($pid=fork()) == 0); $work{$pid}="insert";
test_2() if (($pid=fork()) == 0); $work{$pid}="simple1";
test_3() if (($pid=fork()) == 0); $work{$pid}="funny1";
test_2() if (($pid=fork()) == 0); $work{$pid}="simple2";
test_3() if (($pid=fork()) == 0); $work{$pid}="funny2";
test_2() if (($pid=fork()) == 0); $work{$pid}="simple3";
test_3() if (($pid=fork()) == 0); $work{$pid}="funny3";
test_2() if (($pid=fork()) == 0); $work{$pid}="simple4";
test_3() if (($pid=fork()) == 0); $work{$pid}="funny4";
alter_test() if (($pid=fork()) == 0); $work{$pid}="alter";

$errors=0;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finished with exit code $ret\n";
  $errors++ if ($ret != 0);
}

if (!$opt_skip_delete && !$errors)
{
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  foreach $table (@testtables) {
      $dbh->Query("drop table $table");
  }
}
print ($errors ? "Test failed\n" :"Test ok\n");

$end_time=new Benchmark;
print "Total time: " .
  timestr(timediff($end_time, $start_time),"noc") . "\n";

exit(0);

#
# Insert records in the ?? tables the Nasdaq way
# 

sub test_1
{
    my ($dbh,$table,$tmpvar,$rows,$found,$i);

    $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
    $tmpvar=1;
    $rows=$found=0;
    for ($i=0 ; $i < $opt_loop_count; $i++)
    {
	$tmpvar^= ((($tmpvar + 63) + $i)*3 % $numtables);
	# Nasdaq step 1:
	$sth=$dbh->Query("select id,last from $dtable where id='$tmpvar'")
	    or die "Select directory row: $Mysql::db_errstr\n";
	# Nasdaq step 2:
	my ($did,$dlast) = $sth->FetchRow
	    or die "Fetch directory row: $Mysql::db_errstr\n";
	$dlast++;
	$sth=$dbh->Query("INSERT into $testtables[$did]".
			 " VALUES($dlast,'This is entry $dlast',NULL)")
	    || die "Got error on insert table $testtable[$did]:". 
		" $Mysql::db_errstr\n";
	# Nasdaq step 3 - where my application hangs
	$sth=$dbh->Query("update $dtable set last='$dlast' where id='$tmpvar'")
	    or die "Updating directory for table $testtable[$did]:".
		" Mysql::db_errstr\n";
	$rows++;
    }
    $dbh=0;
    print "Test_1: Inserted $rows rows\n";
    exit(0);
}

#
# Nasdaq simple select
#

sub test_2
{
    my ($dbh,$id,$tmpvar,$rows,$found,$i);
 
    $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
    $rows=$found=0;
    $tmpvar=1;
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
	$tmpvar^= ((($tmpvar + 63) + $i)*3 % $numtables);
	$sth=$dbh->Query("select a.id,a.info from $testtables[$tmpvar] as a,".
			 "$dtable as d".
			 " where a.id=d.last and $i >= 0")
	    || die "Got error select max: $Mysql::db_errstr\n";
	if ((@row = $sth->FetchRow()) && defined($row[0]))
	{
	    $found++;
	}
    }
    $dbh=0;
    print "Test_2: Found $found rows\n";
    exit(0);
}


#
# Nasdaq not-so-simple select
#

sub test_3
{
    my ($dbh,$id,$tmpvar,$rows,$i);
    $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
    $rows=0;
    $tmpvar ||= $numtables;
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
	$tmpvar^= ((($tmpvar + 63) + $i)*3 % $numtables);
	$id1 = ($tmpvar+1) % $numtables;
	$id2 = ($id1+1) % $numtables;
	$id3 = ($id2+1) % $numtables;
	$sth = $dbh->Query("SELECT greatest(a.id, b.id, c.id), a.info".
			   " FROM $testtables[$id1] as a,".
			   " $testtables[$id2] as b,".
			   " $testtables[$id3] as c,".
			   " $dtable as d1, $dtable as d2, $dtable as d3".
			   " WHERE ".
			   " d1.last=a.id AND d2.last=b.id AND d3.last=c.id".
			   " AND d1.id='$id1' AND d2.id='$id2'".
			   " AND d3.id='$id3'")
	    or die "Funny select: $Mysql::db_errstr\n";
	$rows+=$sth->numrows;
    }
    $dbh=0;
    print "Test_3: Found $rows rows\n";
    exit(0);
}

#
# Do an ALTER TABLE every 20 seconds
#

sub alter_test
{
    my ($dbh,$count,$old_row_count,$row_count,$id,@row,$sth);

    $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
    $id=$count=$row_count=0; $old_row_count= -1;

    # Execute the test as long as we get more data into the table
    while ($row_count != $old_row_count)
    {
      sleep(10);
      $sth=$dbh->Query("ALTER TABLE $testtables[$id] modify info varchar(32)") or die "Couldn't execute ALTER TABLE\n";
      $sth=0;
      $id=($id+1) % $numtables;

      # Test if insert test has ended
      $sth=$dbh->query("select count(*) from $testtables[0]") or die "Couldn't execute count(*)\n";
      @row = $sth->FetchRow();
      $old_row_count= $row_count;
      $row_count=$row[0];
      $count++;
    }
    $dbh=0;
    print "alter: Executed $count ALTER TABLE commands\n";
    exit(0);
}
