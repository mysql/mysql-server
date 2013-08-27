#!/usr/bin/perl -w

# Copyright (C) 2002 MySQL AB
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

$opt_loop_count=10000; # Change this to make test harder/easier

##################### Standard benchmark inits ##############################

use DBI;
use Getopt::Long;
use Benchmark;

package main;

$opt_skip_create=$opt_skip_in=$opt_verbose=$opt_fast_insert=
$opt_lock_tables=$opt_debug=$opt_skip_delete=$opt_fast=$opt_force=0;
$opt_threads=2;
$opt_host=$opt_user=$opt_password=""; $opt_db="test";

GetOptions("host=s","db=s","user=s","password=s","loop-count=i","skip-create","skip-in","skip-delete","verbose","fast-insert","lock-tables","debug","fast","force","threads=i") || die "Aborted";
$opt_verbose=$opt_debug=$opt_lock_tables=$opt_fast_insert=$opt_fast=$opt_skip_in=$opt_force=undef;  # Ignore warnings from these

print "Testing truncate from $opt_threads multiple connections $opt_loop_count times\n";

@testtables = ( ["bench_f31", "type=heap"]);

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
	     " (id int(6) not null,".
	     " info varchar(32)," .
	     " marker timestamp," .
	     " flag int not null," .
	     " primary key(id)) $extra")

      or die $DBI::errstr;
  }
}

$dbh->disconnect; $dbh=0;	# Close handler
$|= 1;				# Autoflush

####
#### Start the tests
####

for ($i=0 ; $i < $opt_threads ; $i ++)
{
  test_truncate() if (($pid=fork()) == 0); $work{$pid}="truncate";
}

print "Started $opt_threads threads\n";

$errors=0;
$running_insert_threads=$opt_threads;
while (($pid=wait()) != -1)
{
  $ret=$?/256;
  print "thread '" . $work{$pid} . "' finished with exit code $ret\n";
  --$running_insert_threads;
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

sub test_truncate
{
  my ($dbh,$i,$j,$count,$table_def,$table);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($count=0; $count < $opt_loop_count ; $count++)
  {
    my ($table)= ($testtables[0]->[0]);
    $dbh->do("truncate table $table") || die "Got error on truncate: $DBI::errstr\n";
  }
  $dbh->disconnect; $dbh=0;
  print "Test_truncate: Run $count times\n";
  exit(0);
}
