#!/usr/bin/perl -w

# Copyright (C) 2000 MySQL AB
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
# This is a test with uses processes to insert, select and drop tables.
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

print "Testing 5 multiple connections to a server with 1 insert, 2 drop/rename\n";
print "1 select and 1 flush thread\n";

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
  $dbh->do("drop table if exists $firsttable, ${firsttable}_1, ${firsttable}_2");

  print "Creating table $firsttable in database $opt_db\n";
  $dbh->do("create table $firsttable (id int(6) not null, info varchar(32), marker char(1), primary key(id))") || die $DBI::errstr;
  $dbh->disconnect; $dbh=0;	# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

test_insert() if (($pid=fork()) == 0); $work{$pid}="insert";
test_drop(1) if (($pid=fork()) == 0); $work{$pid}="drop 1";
test_drop(2) if (($pid=fork()) == 0); $work{$pid}="drop 2";
test_select() if (($pid=fork()) == 0); $work{$pid}="select";
test_flush() if (($pid=fork()) == 0); $work{$pid}="flush";

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
  my ($dbh,$i);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($i=0 ; $i < $opt_loop_count; $i++)
  {
    if (!$dbh->do("insert into $firsttable values ($i,'This is entry $i','')"))
    {
      print "Warning; Got error on insert: " . $dbh->errstr . "\n" if (! ($dbh->errstr =~ /doesn't exist/));
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_insert: Inserted $i rows\n";
  exit(0);
}


sub test_drop
{
  my ($id) = @_;
  my ($dbh,$i,$sth,$error_counter,$sleep_time);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  $error_counter=0;
  $sleep_time=2;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    sleep($sleep_time);
    # Check if insert thread is ready
    $sth=$dbh->prepare("select count(*) from $firsttable") || die "Got error on select from $firsttable: $dbh->errstr\n";
    if (!$sth->execute || !(@row = $sth->fetchrow_array()) ||
	!$row[0])
    {
      $sth->finish;
      $sleep_time=1;
      last if ($error_counter++ == 5);
      next;
    }
    $sleep_time=2;
    $sth->finish;

    # Change to use a new table
    $dbh->do("create table ${firsttable}_$id (id int(6) not null, info varchar(32), marker char(1), primary key(id))") || die $DBI::errstr;
    $dbh->do("drop table if exists $firsttable") || die "Got error on drop table: $dbh->errstr\n";
    if (!$dbh->do("alter table ${firsttable}_$id rename to $firsttable"))
    {
      print "Warning; Got error from alter table: " . $dbh->errstr . "\n" if (! ($dbh->errstr =~ /already exist/));
      $dbh->do("drop table if exists ${firsttable}_$id") || die "Got error on drop table: $dbh->errstr\n";
    }
  }
  $dbh->do("drop table if exists $firsttable,${firsttable}_$id") || die "Got error on drop table: $dbh->errstr\n";
  $dbh->disconnect; $dbh=0;
  print "Test_drop: Did a drop $i times\n";
  exit(0);
}


#
# select records
#

sub test_select
{
  my ($dbh,$i,$sth,@row,$error_counter,$sleep_time);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $error_counter=0;
  $sleep_time=3;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    sleep($sleep_time);
    $sth=$dbh->prepare("select sum(t.id) from $firsttable as t,$firsttable as t2") || die "Got error on select: $dbh->errstr;\n";
    if ($sth->execute)
    {
      @row = $sth->fetchrow_array();
      $sth->finish;
      $sleep_time=3;
    }
    else
    {
      print "Warning; Got error from select: " . $dbh->errstr . "\n" if (! ($dbh->errstr =~ /doesn't exist/));
      $sth->finish;
      last if ($error_counter++ == 5);
      $sleep_time=1;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_select: ok\n";
  exit(0);
}

#
# flush records
#

sub test_flush
{
  my ($dbh,$i,$sth,@row,$error_counter,$sleep_time);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  $error_counter=0;
  $sleep_time=5;
  for ($i=0 ; $i < $opt_loop_count ; $i++)
  {
    sleep($sleep_time);
    $sth=$dbh->prepare("select count(*) from $firsttable") || die "Got error on prepar: $dbh->errstr;\n";
    if ($sth->execute)
    {
      @row = $sth->fetchrow_array();
      $sth->finish;
      $sleep_time=5;
      $dbh->do("flush tables $firsttable") || die "Got error on flush table: " . $dbh->errstr . "\n";
    }
    else
    {
      print "Warning; Got error from select: " . $dbh->errstr . "\n" if (! ($dbh->errstr =~ /doesn't exist/));
      $sth->finish;
      last if ($error_counter++ == 5);
      $sleep_time=1;
    }
  }
  $dbh->disconnect; $dbh=0;
  print "Test_select: ok\n";
  exit(0);
}
