#!/usr/bin/perl -w
#
# This is a test with uses 3 processes to insert, delete and select
#

$opt_loop_count=100000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use DBI;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
  $opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_host=""; $opt_db="test";

GetOptions("host=s","db=s","loop-count=i","skip-create","skip-in","skip-delete",
"verbose","fast-insert","lock-tables","debug","fast","force") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$opt_force=undef;  # Ignore warnings from these

print "Testing 3 multiple connections to a server with 1 insert, 1 delete\n";
print "and 1 select connections.\n";

$firsttable  = "bench_f1";

####  
####  Start timeing and start test
####

$start_time=new Benchmark;
if (!$opt_skip_create)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $dbh->do("drop table if exists $firsttable");

  print "Creating table $firsttable in database $opt_db\n";
  $dbh->do("create table $firsttable (id int(6) not null, info varchar(32), marker char(1), primary key(id))") || die $DBI::errstr;
  $dbh->disconnect; $dbh=0;	# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

test_insert() if (($pid=fork()) == 0); $work{$pid}="insert";
test_delete() if (($pid=fork()) == 0); $work{$pid}="delete";
test_select() if (($pid=fork()) == 0); $work{$pid}="select1";

$errors=0;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finnished with exit code $ret\n";
  $errors++ if ($ret != 0);
}

if (!$opt_skip_delete && !$errors)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $dbh->do("drop table $firsttable");
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
  my ($dbh,$i,$sth);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $sth=$dbh->do("insert into $firsttable values ($i,'This is entry $i','')") || die "Got error on insert: $Mysql::db_errstr\n";
    $sth=0;
  }
  $dbh->disconnect; $dbh=0;
  print "Test_insert: Inserted $i rows\n";
  exit(0);
}

sub test_delete
{
  my ($dbh,$i,$sth,@row);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    sleep(5);
    if ($opt_lock_tables)
    {
      $sth=$dbh->do("lock tables $firsttable WRITE") || die "Got error on lock tables $firsttable: $Mysql::db_errstr\n";
    }
    $sth=$dbh->prepare("select count(*) from $firsttable") || die "Got error on select from $firsttable: $dbh->errstr\n";
    $sth->execute || die $dbh->errstr;
    if ((@row = $sth->fetchrow_array()))
    {
      last if (!$row[0]);	# Insert thread is probably ready
    }
    $sth=$dbh->do("delete from $firsttable") || die "Got error on delete from $firsttable: $dbh->errstr;\n";
  }
  $sth=0;
  $dbh->disconnect; $dbh=0;
  print "Test_delete: Deleted all rows $i times\n";
  exit(0);
}


#
# select records
#

sub test_select
{
  my ($dbh,$i,$sth,@row);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    $sth=$dbh->prepare("select count(*) from $firsttable") || die "Got error on select from $firsttable: $dbh->errstr;\n";
    $sth->execute || die $dbh->errstr;
    @row = $sth->fetchrow_array();
    $sth=0;
  }
  $dbh->disconnect; $dbh=0;
  print "Test_select: ok\n";
  exit(0);
}
