#!/usr/bin/perl -w

# This is a test with uses 5 processes to insert, update and select from
# two tables.
# One inserts records in the tables, one updates some record in it and
# the last 3 does different selects on the tables.
#

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
	   "skip-delete","verbose","fast-insert","lock-tables","debug","fast",
	   "force") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$Mysql::db_errstr=$opt_force=undef;  # Ignore warnings from these

print "Testing 5 multiple connections to a server with 1 insert, 1 update\n";
print "and 3 select connections.\n";


$firsttable  = "bench_f1";
$secondtable = "bench_f2";

####  
####  Start timeing and start test
####

$start_time=new Benchmark;
if (!$opt_skip_create)
{
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $Mysql::QUIET = 1;
  $dbh->Query("drop table $firsttable");
  $dbh->Query("drop table $secondtable");
  $Mysql::QUIET = 0;

  print "Creating tables $firsttable and $secondtable in database $opt_db\n";
  $dbh->Query("create table $firsttable (id int(6) not null, info varchar(32), marker char(1), primary key(id))") or die $Mysql::db_errstr;
  $dbh->Query("create table $secondtable (id int(6) not null, row int(3) not null,value double, primary key(id,row))") or die $Mysql::db_errstr;

  $dbh=0;			# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

test_1() if (($pid=fork()) == 0); $work{$pid}="insert";
test_2() if (($pid=fork()) == 0); $work{$pid}="update";
test_3() if (($pid=fork()) == 0); $work{$pid}="select1";
test_4() if (($pid=fork()) == 0); $work{$pid}="select2";
test_5() if (($pid=fork()) == 0); $work{$pid}="select3";

$errors=0;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finnished with exit code $ret\n";
  $errors++ if ($ret != 0);
}

if (!$opt_skip_delete && !$errors)
{
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $dbh->Query("drop table $firsttable");
  $dbh->Query("drop table $secondtable");
}
print ($errors ? "Test failed\n" :"Test ok\n");

$end_time=new Benchmark;
print "Total time: " .
  timestr(timediff($end_time, $start_time),"noc") . "\n";

exit(0);

#
# Insert records in the two tables
# 

sub test_1
{
  my ($dbh,$tmpvar,$rows,$found,$i);

  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $tmpvar=1;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $sth=$dbh->Query("insert into $firsttable values ($i,'This is entry $i','')") || die "Got error on insert: $Mysql::db_errstr\n";
    $row_count=($i % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $sth=$dbh->Query("insert into $secondtable values ($i,$j,0)") || die "Got error on insert: $Mysql::db_errstr\n";
    }
    if (($tmpvar % 10) == 0)
    {
      $sth=$dbh->Query("select max(info) from $firsttable") || die "Got error on select max(info): $Mysql::db_errstr\n";
      $sth=$dbh->Query("select max(value) from $secondtable") || die "Got error on select max(info): $Mysql::db_errstr\n";      
      $found+=2;
    }
  }
  $dbh=0;
  print "Test_1: Inserted $rows rows, found $found rows\n";
  exit(0);
}

#
# Update records in both tables
#

sub test_2
{
  my ($dbh,$id,$tmpvar,$rows,$found,$i,$max_id,$tmp);

  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $tmpvar=111111;
  $rows=$found=$max_id=$id=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmp=(($tmpvar + 63) + $i)*3;
    $tmp=$tmp-int($tmp/100000)*100000; 
    $tmpvar^= $tmp;
    $tmp=$tmpvar - int($tmpvar/10)*10;
    if ($max_id < 2 || $tmp == 0)
    {
      $max_id=0;
      $sth=$dbh->Query("select max(id) from $firsttable where marker=''") || die "Got error select max: $Mysql::db_errstr\n";
      if ((@row = $sth->FetchRow()) && defined($row[0]))
      {
	$found++;
	$max_id=$id=$row[0];
      }
    }
    else
    {
      $id= $tmpvar % ($max_id-1)+1;
    }
    if ($id)
    {
      $sth=$dbh->Query("update $firsttable set marker='x' where id=$id") || die "Got error update $firsttable: $Mysql::db_errstr\n";
      $rows+=$sth->affected_rows;
      if ($sth->affected_rows)
      {
	$sth=$dbh->Query("update $secondtable set value=$i where id=$id") || die "Got error update $firsttable: $Mysql::db_errstr\n";
	$rows+=$sth->affected_rows;
      }
    }
  }
  $dbh=0;
  print "Test_2: Found $found rows, Updated $rows rows\n";
  exit(0);
}


#
# select records
#

sub test_3
{
  my ($dbh,$id,$tmpvar,$rows,$i);
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $tmpvar=222222;
  $rows=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$tmpvar % $opt_loop_count;
    $sth=$dbh->Query("select id from $firsttable where id=$id") || die "Got error on select from $firsttable: $Mysql::db_errstr\n";
    $rows+=$sth->numrows;
  }
  $dbh=0;
  print "Test_3: Found $rows rows\n";
  exit(0);
}


#
# Note that this uses row=1 and in some cases won't find any matching
# records
#

sub test_4
{
  my ($dbh,$id,$tmpvar,$rows,$i);
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $tmpvar=333333;
  $rows=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$tmpvar % $opt_loop_count;
    $sth=$dbh->Query("select id from $secondtable where id=$id") || die "Got error on select form $secondtable: $Mysql::db_errstr\n";
    $rows+=$sth->numrows;
  }
  $dbh=0;
  print "Test_4: Found $rows rows\n";
  exit(0);
}


sub test_5
{
  my ($dbh,$id,$tmpvar,$rows,$i,$max_id);
  $dbh = Mysql->Connect($opt_host, $opt_db) || die $Mysql::db_errstr;
  $tmpvar=444444;
  $rows=$max_id=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    if ($max_id == 0 || ($tmpvar % 10 == 0))
    {
      $sth=$dbh->Query("select max(id) from $firsttable") || die "Got error select max: $Mysql::db_errstr\n";
      if ((@row = $sth->FetchRow()) && defined($row[0]))
      {
	$max_id=$id=$row[0];
      }
      else
      {
	$id=0;
      }
    }
    else
    {
      $id= $tmpvar % $max_id;
    }
    $sth=$dbh->Query("select value from $firsttable,$secondtable where $firsttable.id=$id and $secondtable.id=$firsttable.id") || die "Got error on select form $secondtable: $Mysql::db_errstr\n";
    $rows+=$sth->numrows;
  }
  $dbh=0;
  print "Test_5: Found $rows rows\n";
  exit(0);
}
