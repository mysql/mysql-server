#!/usr/bin/perl -w

# Copyright (C) 2005 MySQL AB
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
# This is a test for a key cache bug (bug #10167)
# To expose the bug mysqld should be started with --key-buffer-size=64K
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
$kill_file= "/tmp/mysqltest_index_corrupt.$$";

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

  print "Creating tables in $opt_db\n";
  $dbh->do("create table $firsttable (
c_pollid    INTEGER  NOT NULL,
c_time      BIGINT   NOT NULL,
c_data      DOUBLE   NOT NULL,
c_error     INTEGER  NOT NULL,
c_warning   INTEGER  NOT NULL,
c_okay      INTEGER  NOT NULL,
c_unknown   INTEGER  NOT NULL,
c_rolled_up BIT      NOT NULL,
INDEX t_mgmt_hist_r_i1 (c_pollid),
INDEX t_mgmt_hist_r_i2 (c_time),
INDEX t_mgmt_hist_r_i3 (c_rolled_up))") or die $DBI::errstr;

  $dbh->do("create table $secondtable (
c_pollid  INTEGER  NOT NULL,
c_min_time  BIGINT   NOT NULL,
c_max_time  BIGINT   NOT NULL,
c_min_data  DOUBLE   NOT NULL,
c_max_data  DOUBLE   NOT NULL,
c_avg_data  DOUBLE   NOT NULL,
c_error     INTEGER  NOT NULL,
c_warning   INTEGER  NOT NULL,
c_okay      INTEGER  NOT NULL,
c_unknown   INTEGER  NOT NULL,
c_rolled_up BIT      NOT NULL,
INDEX t_mgmt_hist_d_i1 (c_pollid),
INDEX t_mgmt_hist_d_i2 (c_min_time),
INDEX t_mgmt_hist_d_i3 (c_max_time),
INDEX t_mgmt_hist_d_i4 (c_rolled_up))") or die $DBI::errstr;


  $dbh->disconnect; $dbh=0;	# Close handler
}
$|= 1;				# Autoflush

####
#### Start the tests
####

print "Running tests\n";
insert_in_bench() if (($pid=fork()) == 0); $work{$pid}="insert";
select_from_bench() if (($pid=fork()) == 0); $work{$pid}="insert-select;
delete_from_bench() if (($pid=fork()) == 0); $work{$pid}="delete";

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
  $dbh->do("drop table $firsttable, $secondtable");
}
print ($errors ? "Test failed\n" :"Test ok\n");

$end_time=new Benchmark;
print "Total time: " .
  timestr(timediff($end_time, $start_time),"noc") . "\n";

unlink $kill_file;

exit(0);

#
# Insert records in the two tables
#

sub insert_in_bench
{
  my ($dbh,$rows,$found,$i);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($rows= 1; $rows <= $opt_loop_count ; $rows++)
  {
    $c_pollid = sprintf("%d",rand 1000);
    $c_time = sprintf("%d",rand 100000);
    $c_data = rand 1000000;
    $test = rand 1;
    $c_error=0;
    $c_warning=0;
    $c_okay=0;
    $c_unknown=0;
    if ($test < .8) {
      $c_okay=1;
    } elsif ($test <.9) {
      $c_error=1;
    } elsif ($test <.95) {
      $c_warning=1;
    } else {
      $c_unknown=1;
    }
    $statement = "INSERT INTO $firsttable (c_pollid, c_time, c_data, c_error
, c_warning, c_okay, c_unknown, c_rolled_up) ".
  "VALUES ($c_pollid,$c_time,$c_data,$c_error,$c_warning,$c_okay,$c_unknown,0)";
    $cursor = $dbh->prepare($statement);
    $cursor->execute();
    $cursor->finish();
  }

  $dbh->disconnect; $dbh=0;
  print "insert_in_bench: Inserted $rows rows\n";

  # Kill other threads
  open(KILLFILE, "> $kill_file");
  close(KILLFILE);

  exit(0);
}


sub select_from_bench
{
  my ($dbh,$rows,$cursor);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;
  for ($rows= 1; $rows < $opt_loop_count ; $rows++)
  {
    $t_value = rand 100000;
    $t_value2 = $t_value+10000;
    $statement = "INSERT INTO $secondtable (c_pollid, c_min_time, c_max_time
, c_min_data, c_max_data, c_avg_data, c_error, c_warning, c_okay, c_unknown, c_rolled_up) SELECT c_pollid, MIN(c_time), MAX(c_time), MIN(c_data), MAX(c_data), AVG(c_data), SUM(c_error), SUM(c_warning), SUM(c_okay), SUM(c_unknown), 0 FROM $firsttable WHERE (c_time>=$t_value) AND (c_time<$t_value2) AND (c_rolled_up=0) GROUP BY c_pollid";
    $cursor = $dbh->prepare($statement);
    $cursor->execute();
    $cursor->finish();
    sleep 1;
    if (-e $kill_file)
    {
      last;
    }
  }
  print "select_from_bench: insert-select executed $rows times\n";
  exit(0);
}


sub delete_from_bench
{
  my ($dbh,$row, $t_value, $t2_value, $statement, $cursor);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host",
		      $opt_user, $opt_password,
		    { PrintError => 0}) || die $DBI::errstr;

  for ($rows= 1; $rows < $opt_loop_count ; $rows++)
  {
    $t_value = rand 50000;
    $t2_value = $t_value + 50001;
    $statement = "DELETE FROM $firsttable WHERE (c_time>$t_value) AND (c_time<$t2_value)";
    $cursor = $dbh->prepare($statement);
    $cursor->execute();
    $cursor->finish();
    sleep 10;
    if (-e $kill_file)
    {
      last;
    }
  }
  print "delete: delete executed $rows times\n";
  exit(0);
}
