#!/usr/bin/perl

# Copyright (C) 2000, 2003 MySQL AB
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

use DBI;
use Benchmark;

$opt_loop_count=100000;		# number of rows/3
$small_loop_count=10;		# Loop for full table retrieval
$range_loop_count=$small_loop_count*50;
$many_keys_loop_count=$opt_loop_count;

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

if ($opt_loop_count < 256)
{
  $opt_loop_count=256;		# Some tests must have some data to work!
}

if ($opt_small_test)
{
  $opt_loop_count/=100;
  $range_loop_count/=10;
  $many_keys_loop_count=$opt_loop_count/10;
}
elsif ($opt_small_tables)
{
  $opt_loop_count=10000;		# number of rows/3
  $many_keys_loop_count=$opt_loop_count;
}
elsif ($opt_small_key_tables)
{
  $many_keys_loop_count/=10;
}

print "Testing the speed difference between some table types\n";

####
#### Generating random keys
####

print "Generating random keys\n";
$random[$opt_loop_count]=0;
for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $random[$i]=$i+$opt_loop_count;
}

my $tmpvar=1;
for ($i=0 ; $i < $opt_loop_count ; $i++)
{
  $tmpvar^= ((($tmpvar + 63) + $i)*3 % $opt_loop_count);
  $swap=$tmpvar % $opt_loop_count;
  $tmp=$random[$i]; $random[$i]=$random[$swap]; $random[$swap]=$tmp;
}

$total_rows=$opt_loop_count*3;

####
####  Connect and start timeing
####
$start_time=new Benchmark;
$dbh = $server->connect();
####
#### Create needed tables
####

$table_name="bench1";
test($table_name,"type=isam","char");
test($table_name,"type=myisam pack_keys=0","char");
test($table_name,"type=myisam pack_keys=0","char");
test($table_name,"type=myisam pack_keys=0 checksum=1","char");
test($table_name,"type=myisam pack_keys=1","char");

test($table_name,"type=isam","varchar");
test($table_name,"type=myisam pack_keys=1","varchar");
test($table_name,"type=myisam pack_keys=0","varchar");
test($table_name,"type=myisam pack_keys=0 checksum=1","varchar");

#test("type=heap","char"); # The default table sizes is a bit big for this one

$dbh->disconnect;
exit (0);

sub test {
  my ($name,$options,$chartype)=@_;

  print "\nTesting with options: '$options'\n";
  $dbh->do("drop table $name");
  do_many($dbh,$server->create("$name",
			       ["id int NOT NULL",
				"id2 int NOT NULL",
				"id3 int NOT NULL",
				"dummy1 $chartype(30)"],
			       ["primary key (id,id2)",
				"index index_id3 (id3)"],
			      $options));

  if ($opt_lock_tables)
  {
    $sth = $dbh->do("LOCK TABLES $name WRITE") || die $DBI::errstr;
  }

  if ($opt_fast && defined($server->{vacuum}))
  {
    $server->vacuum(\$dbh,1);
  }

  ####
  #### Insert $total_rows records in order, in reverse order and random.
  ####

  $loop_time=new Benchmark;

  if ($opt_fast_insert)
  {
    $query="insert into $name values ";
  }
  else
  {
    $query="insert into $name (id,id2,id3,dummy1) values ";
  }

  if (($opt_fast || $opt_fast_insert) && $limits->{'multi_value_insert'})
  {
    $query_size=$server->{'limits'}->{'query_size'};

    print "Inserting $opt_loop_count multiple-value rows in order\n";
    $res=$query;
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $tmp= "($i,$i,$i,'ABCDEFGHIJ'),";
      if (length($tmp)+length($res) < $query_size)
      {
	$res.= $tmp;
      }
      else
      {
	$sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
	$res=$query . $tmp;
      }
    }
    print "Inserting $opt_loop_count multiple-value rows in reverse order\n";
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $tmp= "(" . ($total_rows-1-$i) . "," .($total_rows-1-$i) .
	"," .($total_rows-1-$i) . ",'BCDEFGHIJK'),";
      if (length($tmp)+length($res) < $query_size)
      {
	$res.= $tmp;
      }
      else
      {
	$sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
	$res=$query . $tmp;
      }
    }
    print "Inserting $opt_loop_count multiple-value rows in random order\n";
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $tmp= "(" . $random[$i] . "," . $random[$i] . "," . $random[$i] .
	",'CDEFGHIJKL')," or die $DBI::errstr;
      if (length($tmp)+length($res) < $query_size)
      {
	$res.= $tmp;
      }
      else
      {
	$sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
	$res=$query . $tmp;
      }
    }
    $sth = $dbh->do(substr($res,0,length($res)-1)) or die $DBI::errstr;
  }
  else
  {
    print "Inserting $opt_loop_count rows in order\n";
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $sth = $dbh->do($query . "($i,$i,$i,'ABCDEFGHIJ')") or die $DBI::errstr;
    }

    print "Inserting $opt_loop_count rows in reverse order\n";
    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $sth = $dbh->do($query . "(" . ($total_rows-1-$i) . "," .
		      ($total_rows-1-$i) . "," .
		      ($total_rows-1-$i) . ",'BCDEFGHIJK')")
	or die $DBI::errstr;
    }

    print "Inserting $opt_loop_count rows in random order\n";

    for ($i=0 ; $i < $opt_loop_count ; $i++)
    {
      $sth = $dbh->do($query . "(". $random[$i] . "," . $random[$i] .
		      "," . $random[$i] . ",'CDEFGHIJKL')") or die $DBI::errstr;
    }
  }

  $end_time=new Benchmark;
  print "Time for insert (" . ($total_rows) . "): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

  if ($opt_fast && defined($server->{vacuum}))
  {
    $server->vacuum(\$dbh,1);
  }

  $sth=$dbh->prepare("show table status like '$name'");
  $sth->execute || die "Show table status returned error: $DBI::errstr\n";
  while (@row = $sth->fetchrow_array)
  {
    print join("|  ",@row) . " |\n";
  }
  $dbh->do("drop table $name") if (!$opt_skip_delete);
}
