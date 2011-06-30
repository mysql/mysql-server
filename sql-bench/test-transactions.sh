#!/usr/bin/perl
# Copyright (c) 2001, 2003, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA
#
# Test of transactions performance.
#

##################### Standard benchmark inits ##############################

use Cwd;
use DBI;
use Benchmark;
#use warnings;

$opt_groups=27;		    # Characters are 'A' -> Z

$opt_loop_count=10000;	    # Change this to make test harder/easier
$opt_medium_loop_count=100; # Change this to make test harder/easier

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

# Avoid warnings for variables in bench-init.pl
# (Only works with perl 5.6)
#our ($opt_small_test, $opt_small_tables, $opt_debug, $opt_force);

if ($opt_small_test || $opt_small_tables)
{
  $opt_loop_count/=100;
  $opt_medium_loop_count/=10;
}


if (!$server->{transactions} && !$opt_force)
{
  print "Test skipped because the database doesn't support transactions\n";
  exit(0);
}

####
####  Connect and start timeing
####

$start_time=new Benchmark;
$dbh = $server->connect();

###
### Create Table
###

print "Creating tables\n";
$dbh->do("drop table bench1");
$dbh->do("drop table bench2");

do_many($dbh,$server->create("bench1",
			     ["idn int NOT NULL",
			      "rev_idn int NOT NULL",
			      "region char(1) NOT NULL",
			      "grp int NOT NULL",
			      "updated tinyint NOT NULL"],
			     ["primary key (idn)",
			      "unique (region,grp)"]));
do_many($dbh,$server->create("bench2",
			     ["idn int NOT NULL",
			      "rev_idn int NOT NULL",
			      "region char(1) NOT NULL",
			      "grp int NOT NULL",
			      "updated tinyint NOT NULL"],
			     ["primary key (idn)",
			      "unique (region,grp)"]));

$dbh->{AutoCommit} = 0;

###
### Test insert perfomance
###

test_insert("bench1","insert_commit",0);
test_insert("bench2","insert_autocommit",1);

sub test_insert
{
  my ($table, $test_name, $auto_commit)= @_;
  my ($loop_time,$end_time,$id,$rev_id,$grp,$region);

  $dbh->{AutoCommit}= $auto_commit;
  $loop_time=new Benchmark;

  for ($id=0,$rev_id=$opt_loop_count-1 ; $id < $opt_loop_count ;
       $id++,$rev_id--)
  {
    $grp=$id/$opt_groups;
    $region=chr(65+$id%$opt_groups);
    do_query($dbh,"insert into $table values ($id,$rev_id,'$region',$grp,0)");
  }

  $dbh->commit if (!$auto_commit);
  $end_time=new Benchmark;
  print "Time for $test_name  ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

###
### Test rollback performance
###

print "Test transactions rollback performance\n" if($opt_debug);

##
## Insert rollback test
##

#
# Test is done by inserting 100 rows in a table with lots of rows and
# then doing a rollback on these
#

{
  my ($id,$rev_id,$grp,$region,$end,$loop_time,$end_time,$commit_loop,$count);

  $dbh->{AutoCommit} = 0;
  $loop_time=new Benchmark;
  $end=$opt_loop_count*2;
  $count=0;

  for ($commit_loop=1, $id=$opt_loop_count ; $id < $end ;
       $id++, $commit_loop++)
  {
    $rev_id=$end-$id;
    $grp=$id/$opt_groups;
    $region=chr(65+$id%$opt_groups);
    do_query($dbh,"insert into bench1 values ($id,$rev_id,'$region',$grp,0)");
    if ($commit_loop >= $opt_medium_loop_count)
    {
      $dbh->rollback;
      $commit_loop=0;
      $count++;
    }
  }
  if ($commit_loop > 1)
  {
    $dbh->rollback;
    $count++;
  }
  $end_time=new Benchmark;
  print "Time for insert_rollback ($count:$opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

##
## Update rollback test
##

#
# Test is done by updating 100 rows in a table with lots of rows and
# then doing a rollback on these
#

{
  my ($id,$loop_time,$end_time,$commit_loop,$count);

  $dbh->{AutoCommit} = 0;
  $loop_time=new Benchmark;
  $end=$opt_loop_count*2;
  $count=0;

  for ($commit_loop=1, $id=0 ; $id < $opt_loop_count ; $id++, $commit_loop++)
  {
    do_query($dbh,"update bench1 set updated=2 where idn=$id");
    if ($commit_loop >= $opt_medium_loop_count)
    {
      $dbh->rollback;
      $commit_loop=0;
      $count++;
    }
  }
  if ($commit_loop > 1)
  {
    $dbh->rollback;
    $count++;
  }
  $end_time=new Benchmark;
  print "Time for update_rollback ($count:$opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

##
## Delete rollback test
##

#
# Test is done by deleting 100 rows in a table with lots of rows and
# then doing a rollback on these
#

{
  my ($id,$loop_time,$end_time,$commit_loop,$count);

  $dbh->{AutoCommit} = 0;
  $loop_time=new Benchmark;
  $end=$opt_loop_count*2;
  $count=0;

  for ($commit_loop=1, $id=0 ; $id < $opt_loop_count ; $id++, $commit_loop++)
  {
    do_query($dbh,"delete from bench1 where idn=$id");
    if ($commit_loop >= $opt_medium_loop_count)
    {
      $dbh->rollback;
      $commit_loop=0;
      $count++;
    }
  }
  if ($commit_loop > 1)
  {
    $dbh->rollback;
    $count++;
  }
  $end_time=new Benchmark;
  print "Time for delete_rollback ($count:$opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}


###
### Test update perfomance
###

test_update("bench1","update_commit",0);
test_update("bench2","update_autocommit",1);

sub test_update
{
  my ($table, $test_name, $auto_commit)= @_;
  my ($loop_time,$end_time,$id);

  $dbh->{AutoCommit}= $auto_commit;
  $loop_time=new Benchmark;

  for ($id=0 ; $id < $opt_loop_count ; $id++)
  {
    do_query($dbh,"update $table set updated=1 where idn=$id");
  }

  $dbh->commit if (!$auto_commit);
  $end_time=new Benchmark;
  print "Time for $test_name  ($opt_loop_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

###
### Test delete perfomance
###

test_delete("bench1","delete_commit",0);
test_delete("bench2","delete_autocommit",1);

sub test_delete
{
  my ($table, $test_name, $auto_commit)= @_;
  my ($loop_time,$end_time,$id);

  $dbh->{AutoCommit}= $auto_commit;
  $loop_time=new Benchmark;

  for ($id=0 ; $id < $opt_loop_count ; $id++)
 {
    do_query($dbh,"delete from $table where idn=$id");
  }
  $dbh->commit if (!$auto_commit);
  $end_time=new Benchmark;
  print "Time for $test_name  ($opt_loop_count): " .
   timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

####
#### End of benchmark
####

$sth = $dbh->do("drop table bench1" . $server->{'drop_attr'}) or die $DBI::errstr;
$sth = $dbh->do("drop table bench2" . $server->{'drop_attr'}) or die $DBI::errstr;

$dbh->disconnect;				# close connection
end_benchmark($start_time);
