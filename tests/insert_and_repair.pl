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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#
# This is a test of insert and repair/check.
#

$opt_loop_count=100000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use DBI;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
  $opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_host=$opt_user=$opt_password=""; $opt_db="test";

GetOptions("host=s","db=s","loop-count=i","skip-create","skip-in",
	   "skip-delete","verbose","fast-insert","lock-tables","debug","fast",
	   "force","user=s","password=s") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$opt_force=undef;  # Ignore warnings from these

$firsttable  = "bench_f1";
$secondtable = "bench_f2";

####  
####  Start timeing and start test
####

$start_time=new Benchmark;
if (!$opt_skip_create)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $dbh->do("drop table if exists $firsttable, $secondtable");

  print "Creating tables $firsttable and $secondtable in database $opt_db\n";
  $dbh->do("create table $firsttable (id int(7) not null, thread tinyint not null, info varchar(32), marker char(1), primary key(id,thread))") or die $DBI::errstr;
  $dbh->do("create table $secondtable (id int(7) not null, thread tinyint not null, row int(3) not null,value double, primary key(id,thread,row)) delay_key_write=1") or die $DBI::errstr;
  $dbh->disconnect; $dbh=0;	# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

insert_in_bench1() if (($pid=fork()) == 0); $work{$pid}="insert in bench1";
insert_in_bench2() if (($pid=fork()) == 0); $work{$pid}="insert in bench2";
repair_and_check() if (($pid=fork()) == 0); $work{$pid}="repair/check";

$errors=0;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finished with exit code $ret\n";
  $errors++ if ($ret != 0);
}

if (!$opt_skip_delete && !$errors)
{
  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $dbh->do("drop table $firsttable,$secondtable");
}
print ($errors ? "Test failed\n" :"Test ok\n");

$end_time=new Benchmark;
print "Total time: " .
  timestr(timediff($end_time, $start_time),"noc") . "\n";

exit(0);

#
# Insert records in the two tables
# 

sub insert_in_bench1
{
  my ($dbh,$rows,$found,$i);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $sth=$dbh->do("insert into $firsttable values ($i,0,'This is entry $i','')") || die "Got error on insert: $DBI::errstr\n";
    $row_count=($i % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $sth=$dbh->do("insert into $secondtable values ($i,0,$j,0)") || die "Got error on insert: $DBI::errstr\n";
    }
  }
  $dbh->disconnect; $dbh=0;
  print "insert_in_bench1: Inserted $rows rows\n";
  exit(0);
}

sub insert_in_bench2
{
  my ($dbh,$rows,$found,$i);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $rows=$found=0;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    $sth=$dbh->do("insert into $firsttable values ($i,1,'This is entry $i','')") || die "Got error on insert: $DBI::errstr\n";
    $row_count=((7-$i) % 7)+1;
    $rows+=1+$row_count;
    for ($j=0 ; $j < $row_count; $j++)
    {
      $sth=$dbh->do("insert into $secondtable values ($i,1,$j,0)") || die "Got error on insert: $DBI::errstr\n";
    }
  }
  $dbh->disconnect; $dbh=0;
  print "insert_in_bench2: Inserted $rows rows\n";
  exit(0);
}


sub repair_and_check
{
  my ($dbh,$row,@row,$found1,$found2,$last_found1,$last_found2,$i,$type,
      $table);
  $found1=$found2=0; $last_found1=$last_found2= -1;

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($i=0; $found1 != $last_found1 && $found2 != $last_found1 ; $i++)
  {
    $type=($i & 2) ? "repair" : "check";
    if ($i & 1)
    {
      $table=$firsttable;
      $last_found1=$found1;
    }
    else
    {
      $table=$secondtable;
      $last_found2=$found2;
    }
    $sth=$dbh->prepare("$type table $table") || die "Got error on prepare: $dbh->errstr\n";
    $sth->execute || die $dbh->errstr;    

    while (($row=$sth->fetchrow_arrayref))
    {
      if ($row->[3] ne "OK")
      {
	print "Got error " . $row->[3] . " when doing $type on $table\n";
	exit(1);
      }
    }
    $sth=$dbh->prepare("select count(*) from $table") || die "Got error on prepare: $dbh->errstr\n";
    $sth->execute || die $dbh->errstr;
    @row = $sth->fetchrow_array();
    if ($i & 1)
    {
      $found1= $row[0];
    }
    else
    {
      $found2= $row[0];
    }
    $sth->finish;
    sleep(2);
  }
  $dbh->disconnect; $dbh=0;
  print "check/repair: Did $i repair/checks\n";
  exit(0);
}
