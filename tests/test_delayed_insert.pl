#!/usr/bin/perl -w

# This is a test for INSERT DELAYED
#

$opt_loop_count=10000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use DBI;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
  $opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_host=$opt_user=$opt_password=""; $opt_db="test";

GetOptions("host=s","db=s","loop-count=i","skip-create","skip-in","skip-delete",
"verbose","fast-insert","lock-tables","debug","fast","force") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$opt_force=undef;  # Ignore warnings from these

print "Testing 8 multiple connections to a server with 1 insert, 2 delayed\n";
print "insert, 1 update, 1 delete, 1 flush tables and 3 select connections.\n";

$firsttable  = "bench_f1";
$secondtable = "bench_f2";

####  
####  Start timeing and start test
####

$start_time=new Benchmark;
if (!$opt_skip_create)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $Mysql::QUIET = 1;
  $dbh->do("drop table if exists $firsttable,$secondtable");
  $Mysql::QUIET = 0;

  print "Creating tables $firsttable and $secondtable in database $opt_db\n";
  $dbh->do("create table $firsttable (id int(6) not null, info varchar(32), marker char(1), primary key(id))") or die $DBI::errstr;
  $dbh->do("create table $secondtable (id int(6) not null, row int(3) not null,value double, primary key(id,row))") or die $DBI::errstr;
  
  $dbh->disconnect;
}
$|= 1;				# Autoflush

####
#### Start the tests
####

test_1() if (($pid=fork()) == 0); $work{$pid}="insert";
test_delayed_1() if (($pid=fork()) == 0); $work{$pid}="delayed_insert1";
test_delayed_2() if (($pid=fork()) == 0); $work{$pid}="delayed_insert2";
test_2() if (($pid=fork()) == 0); $work{$pid}="update";
test_3() if (($pid=fork()) == 0); $work{$pid}="select1";
test_4() if (($pid=fork()) == 0); $work{$pid}="select2";
test_5() if (($pid=fork()) == 0); $work{$pid}="select3";
test_del() if (($pid=fork()) == 0); $work{$pid}="delete";
test_flush() if (($pid=fork()) == 0); $work{$pid}="flush";

$errors=0;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finnished with exit code $ret\n";
  $errors++ if ($ret != 0);
}

if (!$opt_skip_delete && !$errors)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $dbh->do("drop table $firsttable");
  $dbh->do("drop table $secondtable");
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

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=1;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $dbh->do("insert into $firsttable values ($i,'This is entry $i','')") || die "Got error on insert: $DBI::errstr\n";
    $row_count=($i % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $dbh->do("insert into $secondtable values ($i,$j,0)") || die "Got error on insert: $DBI::errstr\n";
    }
  }
  $dbh->disconnect;
  print "Test_1: Inserted $rows rows\n";
  exit(0);
}


sub test_delayed_1
{
  my ($dbh,$tmpvar,$rows,$found,$i,$id);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=1;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$i+$opt_loop_count;
    $dbh->do("insert delayed into $firsttable values ($id,'This is entry $id','')") || die "Got error on insert: $DBI::errstr\n";
    $row_count=($i % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $dbh->do("insert into $secondtable values ($id,$j,0)") || die "Got error on insert: $DBI::errstr\n";
    }
    if (($tmpvar % 100) == 0)
    {
      $dbh->do("select max(info) from $firsttable") || die "Got error on select max(info): $DBI::errstr\n";
      $dbh->do("select max(value) from $secondtable") || die "Got error on select max(info): $DBI::errstr\n";      
      $found+=2;
    }
  }
  $dbh->disconnect;
  print "Test_1: Inserted delayed $rows rows, found $found rows\n";
  exit(0);
}


sub test_delayed_2
{
  my ($dbh,$tmpvar,$rows,$found,$i,$id);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=1;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$i+$opt_loop_count*2;
    $dbh->do("insert delayed into $firsttable values ($id,'This is entry $id','')") || die "Got error on insert: $DBI::errstr\n";
    $row_count=($i % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $dbh->do("insert delayed into $secondtable values ($id,$j,0)") || die "Got error on insert: $DBI::errstr\n";
    }
    if (($tmpvar % 100) == 0)
    {
      $dbh->do("select max(info) from $firsttable") || die "Got error on select max(info): $DBI::errstr\n";
      $dbh->do("select max(value) from $secondtable") || die "Got error on select max(info): $DBI::errstr\n";      
      $found+=2;
    }
  }
  $dbh->disconnect;
  print "Test_1: Inserted delayed $rows rows, found $found rows\n";
  exit(0);
}

#
# Update records in both tables
#

sub test_2
{
  my ($dbh,$id,$tmpvar,$rows,$found,$i,$max_id,$tmp,$sth,$count);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=111111;
  $rows=$found=$max_id=$id=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmp=(($tmpvar + 63) + $i)*3;
    $tmp=$tmp-int($tmp/100000)*100000; 
    $tmpvar^= $tmp;
    $tmp=$tmpvar - int($tmpvar/10)*10;
    if ($max_id*$tmp == 0)
    {
      $max_id=0;
      $sth=$dbh->prepare("select max(id) from $firsttable where marker=''");
      $sth->execute() || die "Got error select max: $DBI::errstr\n";
      if ((@row = $sth->fetchrow_array()) && defined($row[0]))
      {
	$found++;
	$max_id=$id=$row[0];
      }
      $sth->finish;
    }
    else
    {
      $id= $tmpvar % ($max_id-1)+1;
    }
    if ($id)
    {
      ($count=$dbh->do("update $firsttable set marker='x' where id=$id")) || die "Got error update $firsttable: $DBI::errstr\n";
      $rows+=$count;
      if ($count > 0)
      {
	$count=$dbh->do("update $secondtable set value=$i where id=$id") || die "Got error update $firsttable: $DBI::errstr\n";
	$rows+=$count;
      }
    }
  }
  $dbh->disconnect;
  print "Test_2: Found $found rows, Updated $rows rows\n";
  exit(0);
}


#
# select records
#

sub test_3
{
  my ($dbh,$id,$tmpvar,$rows,$i,$count);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=222222;
  $rows=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$tmpvar % $opt_loop_count;
    $count=$dbh->do("select id from $firsttable where id=$id") || die "Got error on select from $firsttable: $DBI::errstr\n";
    $rows+=$count;
  }
  $dbh->disconnect;
  print "Test_3: Found $rows rows\n";
  exit(0);
}


#
# Note that this uses row=1 and in some cases won't find any matching
# records
#

sub test_4
{
  my ($dbh,$id,$tmpvar,$rows,$i,$count);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=333333;
  $rows=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    $id=$tmpvar % $opt_loop_count;
    $count=$dbh->do("select id from $secondtable where id=$id") || die "Got error on select from $secondtable: $DBI::errstr\n";
    $rows+=$count;
  }
  $dbh->disconnect;
  print "Test_4: Found $rows rows\n";
  exit(0);
}


sub test_5
{
  my ($dbh,$id,$tmpvar,$rows,$i,$max_id,$count,$sth);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $tmpvar=444444;
  $rows=$max_id=0;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $tmpvar^= ((($tmpvar + 63) + $i)*3 % 100000);
    if ($max_id == 0 || ($tmpvar % 10 == 0))
    {
      $sth=$dbh->prepare("select max(id) from $firsttable");
      $sth->execute() || die "Got error select max: $DBI::errstr\n";
      if ((@row = $sth->fetchrow_array()) && defined($row[0]))
      {
	$max_id=$id=$row[0];
      }
      else
      {
	$id=0;
      }
      $sth->finish;
    }
    else
    {
      $id= $tmpvar % $max_id;
    }
    $count=$dbh->do("select value from $firsttable,$secondtable where $firsttable.id=$id and $secondtable.id=$firsttable.id") || die "Got error on select from $secondtable: $DBI::errstr\n";
    $rows+=$count;
  }
  $dbh->disconnect;
  print "Test_5: Found $rows rows\n";
  exit(0);
}


#
# Delete the smallest row
#

sub test_del
{
  my ($dbh,$min_id,$i,$sth,$rows);
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host") || die $DBI::errstr;
  $rows=0;
  for ($i=0 ; $i < $opt_loop_count/3; $i++)
  {
    $sth=$dbh->prepare("select min(id) from $firsttable");
    $sth->execute() || die "Got error on select from $firsttable: $DBI::errstr\n";
    if ((@row = $sth->fetchrow_array()) && defined($row[0]))
    {
      $min_id=$row[0];
    }
    $sth->finish;
    $dbh->do("delete from $firsttable where id = $min_id") || die "Got error on DELETE from $firsttable: $DBI::errstr\n";
    $rows++;
  }
  $dbh->disconnect;
  print "Test_del: Deleted $rows rows\n";
  exit(0);
}


#
# Do a flush tables once in a while
#

sub test_flush
{
  my ($dbh,$sth,$found1,$last_found1,$i,@row);
  $found1=0; $last_found1=-1;

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($i=0; $found1 != $last_found1 ; $i++)
  {
    $sth=$dbh->prepare("flush tables") || die "Got error on prepare: $dbh->errstr\n";
    $sth->execute || die $dbh->errstr;    
    $sth->finish;

    $sth=$dbh->prepare("select count(*) from $firsttable") || die "Got error on prepare: $dbh->errstr\n";
    $sth->execute || die $dbh->errstr;
    @row = $sth->fetchrow_array();
    $last_found1=$found1;
    $found1= $row[0];
    $sth->finish;
    sleep(5);
  }
  $dbh->disconnect; $dbh=0;
  print "flush: Did $i repair/checks\n";
  exit(0);
}
