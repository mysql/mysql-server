#!/usr/bin/perl
# Copyright (c) 2000, 2001, 2003, 2006 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA
#

use Cwd;
use DBI;
use Benchmark;

$opt_loop_count=10;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

$into_table = "";

if ($opt_small_test)
{
  $opt_loop_count/=5;
}

####
####  Connect and start timeing
####

$dbh = $server->connect();
$start_time=new Benchmark;

####
#### Create needed tables
####

init_data();
init_query();

print "Wisconsin benchmark test\n\n";

if ($opt_skip_create)
{
  if ($opt_lock_tables)
  {
    @tmp=@table_names; push(@tmp,@extra_names);
    $sth = $dbh->do("LOCK TABLES " . join(" WRITE,", @tmp) . " WRITE") ||
      die $DBI::errstr;
  }
  goto start_benchmark;
}

$loop_time= new Benchmark;
for($ti = 0; $ti <= $#table_names; $ti++)
{
  my $table_name = $table_names[$ti];
  my $array_ref = $tables[$ti];
  
  # This may fail if we have no table so do not check answer
  $sth = $dbh->do("drop table $table_name" . $server->{'drop_attr'});
  print "Creating table $table_name\n" if ($opt_verbose);
  do_many($dbh,@$array_ref);
}
$end_time=new Benchmark;
print "Time for create_table ($#tables): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}


####
#### Insert data
####

print "Inserting data\n";
$loop_time= new Benchmark;
$row_count=0;
if ($opt_lock_tables)
{
  @tmp=@table_names; push(@tmp,@extra_names);
  $sth = $dbh->do("LOCK TABLES " . join(" WRITE,", @tmp) . " WRITE") ||
    die $DBI::errstr;
}

if ($opt_fast && $server->{'limits'}->{'load_data_infile'})
{
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    if ($table_name =~ /tenk|Bprime/i) {
      $filename = "$pwd/Data/Wisconsin/tenk.data";
    } else {
      $filename = "$pwd/Data/Wisconsin/$table_name.data";
    }
    $row_count+=$server->insert_file($table_name,$filename,$dbh);
  }
}
else
{
  if ($opt_fast && $server->{transactions})
  {
    $dbh->{AutoCommit} = 0;
  }

  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    my $array_ref = $tables[$ti];
    my @table = @$array_ref;
    my $insert_start = "insert into $table_name values (";

    if ($table_name =~ /tenk|Bprime/i) {
      $filename = "$pwd/Data/Wisconsin/tenk.data";
    } else {
      $filename = "$pwd/Data/Wisconsin/$table_name.data";
    }
    open(DATA, "$filename") || die "Can't open text file: $filename\n";
    while(<DATA>)
    {
      chomp;
      $command = $insert_start . $_ . ")";
      print "$command\n" if ($opt_debug);
      $sth = $dbh->do($command) or die $DBI::errstr;
      $row_count++;
    }
  }
  close(DATA);
}

if ($opt_lock_tables)
{
  do_query($dbh,"UNLOCK TABLES");
}
if ($opt_fast && $server->{transactions})
{
  $dbh->commit;
  $dbh->{AutoCommit} = 1;
}

$end_time=new Benchmark;
print "Time to insert ($row_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n";

## Oracle runs out of rollback segments here if using the default "small"
## configuration so disconnect and reconnect to use a new segment
if ($server->small_rollback_segment())
{
  $dbh->disconnect;				# close connection
  $dbh=$server->connect();
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh,@table_names);
}

if ($opt_lock_tables)
{
  @tmp=@table_names; push(@tmp,@extra_names);
  $sth = $dbh->do("LOCK TABLES " . join(" WRITE,", @tmp) . " WRITE") ||
  die $DBI::errstr;
}

$loop_time= $end_time;
print "Delete from Bprime where unique2 >= 1000\n" if ($opt_debug);
$sth = $dbh->do("delete from Bprime where Bprime.unique2 >= 1000") or
  die $DBI::errstr;
$end_time=new Benchmark;
print "Time to delete_big (1): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

####
#### Running the benchmark
####

start_benchmark:

print "Running the actual benchmark\n";

$loop_time= new Benchmark;
$count=0;
for ($i = 0; $i <= $#query; $i+=2)
{
  if ($query[$i+1])				# If the server can handle it
  {
    $loop_count = 1;
    $loop_count = $opt_loop_count if ($query[$i] =~ /^select/i);
    $query[$i] =~ s/\sAS\s+[^\s,]+//ig if (!$limits->{'column_alias'});
    timeit($loop_count, "fetch_all_rows(\$dbh,\"$query[$i]\")");
    $count+=$loop_count;
  }
}

$end_time=new Benchmark;
print "Time for wisc_benchmark ($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

if (!$opt_skip_delete)
{
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    $sth = $dbh->do("drop table $table_name" . $server->{'drop_attr'});
  }
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

####
#### The END
####

$dbh->disconnect;                               # close connection
end_benchmark($start_time);


################################
###### subroutine for database structure
################################

sub init_data
{
  @onek=
    $server->create("onek",
		    ["unique1 int(5) NOT NULL",
		     "unique2 int(4) NOT NULL",
		     "two int(4)",
		     "four int(4)",
		     "ten int(4)",
		     "twenty int(4)",
		     "hundred int(4) NOT NULL",
		     "thousand int(4)",
		     "twothousand int(4)",
		     "fivethous int(4)",
		     "tenthous int(4)",
		     "odd int(4)",
		     "even int(4)",
		     "stringu1 char(16)",
		     "stringu2 char(16)",
		     "string4 char(16)"],
		    ["UNIQUE (unique1)",
		     "UNIQUE (unique2)",
		     "INDEX hundred1 (hundred)"]);

  @tenk1=
    $server->create("tenk1",
		    ["unique1 int(4) NOT NULL",
		     "unique2 int(4) NOT NULL",
		     "two int(4)",
		     "four int(4)",
		     "ten int(4)",
		     "twenty int(4)",
		     "hundred int(4) NOT NULL",
		     "thousand int(4)",
		     "twothousand int(4)",
		     "fivethous int(4)",
		     "tenthous int(4)",
		     "odd int(4)",
		     "even int(4)",
		     "stringu1 char(16)",
		     "stringu2 char(16)",
		     "string4 char(16)"],
		    ["UNIQUE (unique1)",
		     "UNIQUE (unique2)",
		     "INDEX hundred2 (hundred)"]);

  @tenk2=
    $server->create("tenk2",
		    ["unique1 int(4) NOT NULL",
		     "unique2 int(4) NOT NULL",
		     "two int(4)",
		     "four int(4)",
		     "ten int(4)",
		     "twenty int(4)",
		     "hundred int(4) NOT NULL",
		     "thousand int(4)",
		     "twothousand int(4)",
		     "fivethous int(4)",
		     "tenthous int(4)",
		     "odd int(4)",
		     "even int(4)",
		     "stringu1 char(16)",
		     "stringu2 char(16)",
		     "string4 char(16)"],
		    ["UNIQUE (unique1)",
		     "UNIQUE (unique2)",
		     "INDEX hundred3 (hundred)"]);

  @Bprime=
    $server->create("Bprime",
		    ["unique1 int(4) NOT NULL",
		     "unique2 int(4) NOT NULL",
		     "two int(4)",
		     "four int(4)",
		     "ten int(4)",
		     "twenty int(4)",
		     "hundred int(4) NOT NULL",
		     "thousand int(4)",
		     "twothousand int(4)",
		     "fivethous int(4)",
		     "tenthous int(4)",
		     "odd int(4)",
		     "even int(4)",
		     "stringu1 char(16)",
		     "stringu2 char(16)",
		     "string4 char(16)"],
		    ["UNIQUE (unique1)",
		     "UNIQUE (unique2)",
		     "INDEX hundred4 (hundred)"]);

  @tables =
    (\@onek, \@tenk1, \@tenk2, \@Bprime);

  @table_names =
    ("onek", "tenk1", "tenk2", "Bprime");

# Alias used in joins
  @extra_names=
    ("tenk1 as t", "tenk1 as t1","tenk1 as t2", "Bprime as B","onek as o");
}


sub init_query
{
  @query=
    ("select * $into_table from tenk1 where (unique2 > 301) and (unique2 < 402)",1,
     "select * $into_table from tenk1 where (unique1 > 647) and (unique1 < 1648)",1,
     "select * from tenk1 where unique2 = 2001",1,
     "select * from tenk1 where (unique2 > 301) and (unique2 < 402)",1,
     "select t1.*, t2.unique1 AS t2unique1, t2.unique2 AS t2unique2, t2.two AS t2two, t2.four AS t2four, t2.ten AS t2ten, t2.twenty AS t2twenty, t2.hundred AS t2hundred, t2.thousand AS t2thousand, t2.twothousand AS t2twothousand, t2.fivethous AS t2fivethous, t2.tenthous AS t2tenthous, t2.odd AS t2odd, t2.even AS t2even, t2.stringu1 AS t2stringu1, t2.stringu2 AS t2stringu2, t2.string4 AS t2string4 $into_table from tenk1 t1, tenk1 t2 where (t1.unique2 = t2.unique2) and (t2.unique2 < 1000)",$limits->{'table_wildcard'},
     "select t.*,B.unique1 AS Bunique1,B.unique2 AS Bunique2,B.two AS Btwo,B.four AS Bfour,B.ten AS Bten,B.twenty AS Btwenty,B.hundred AS Bhundred,B.thousand AS Bthousand,B.twothousand AS Btwothousand,B.fivethous AS Bfivethous,B.tenthous AS Btenthous,B.odd AS Bodd,B.even AS Beven,B.stringu1 AS Bstringu1,B.stringu2 AS Bstringu2,B.string4 AS Bstring4 $into_table from tenk1 t, Bprime B where t.unique2 = B.unique2",$limits->{'table_wildcard'},
     "select t1.*,o.unique1 AS ounique1,o.unique2 AS ounique2,o.two AS otwo,o.four AS ofour,o.ten AS oten,o.twenty AS otwenty,o.hundred AS ohundred,o.thousand AS othousand,o.twothousand AS otwothousand,o.fivethous AS ofivethous,o.tenthous AS otenthous,o.odd AS oodd, o.even AS oeven,o.stringu1 AS ostringu1,o.stringu2 AS ostringu2,o.string4 AS ostring4 $into_table from onek o, tenk1 t1, tenk1 t2 where (o.unique2 = t1.unique2) and (t1.unique2 = t2.unique2) and (t1.unique2 < 1000) and (t2.unique2 < 1000)",$limits->{'table_wildcard'},
     "select two, four, ten, twenty, hundred, string4 $into_table from tenk1",1,
     "select * $into_table from onek",1,
     "select MIN(unique2) as x $into_table from tenk1",$limits->{'group_functions'},
     "insert into tenk1 (unique1, unique2, two, four, ten, twenty, hundred, thousand, twothousand, fivethous, tenthous, odd, even,stringu1,stringu2, string4) values (10001, 74001, 0, 2, 0, 10, 50, 688, 1950, 4950, 9950, 1, 100, 'ron may choi','jae kwang choi', 'u. c. berkeley')",1,
     "insert into tenk1 (unique1, unique2, two, four, ten, twenty, hundred, thousand, twothousand, fivethous, tenthous, odd, even,stringu1,stringu2, string4) values (19991, 60001, 0, 2, 0, 10, 50, 688, 1950, 4950, 9950, 1, 100, 'ron may choi','jae kwang choi', 'u. c. berkeley')",1,
     "delete from tenk1 where tenk1.unique2 = 877",1,
     "delete from tenk1 where tenk1.unique2 = 876",1,
     "update tenk1 set unique2 = 10001 where tenk1.unique2 =1491",1,
     "update tenk1 set unique2 = 10023 where tenk1.unique2 =1480",1,
     "insert into tenk1 (unique1, unique2, two, four, ten, twenty, hundred, thousand, twothousand, fivethous, tenthous, odd, even, stringu1, stringu2, string4) values (20002, 70002, 0, 2, 0, 10, 50, 688, 1950, 4950, 9950, 1, 100, 'ron may choi', 'jae kwang choi', 'u. c. berkeley')",1,
     "insert into tenk1 (unique1, unique2, two, four, ten, twenty, hundred, thousand, twothousand, fivethous, tenthous, odd, even, stringu1, stringu2, string4) values (50002, 40002, 0, 2, 0, 10, 50, 688, 1950, 4950, 9950, 1, 100, 'ron may choi', 'jae kwang choi', 'u. c. berkeley')",1,
     "delete from tenk1 where tenk1.unique2 = 10001",1,
     "delete from tenk1 where tenk1.unique2 = 900",1,
     "update tenk1 set unique2 = 10088 where tenk1.unique2 =187",1,
     "update tenk1 set unique2 = 10003 where tenk1.unique2 =2000",1,
     "update tenk1 set unique2 = 10020 where tenk1.unique2 =1974",1,
     "update tenk1 set unique2 = 16001 where tenk1.unique2 =1140",1,
     );
}
