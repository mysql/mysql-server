#!@PERL@
# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# Test of extreme tables.
#

##################### Standard benchmark inits ##############################

use DBI;
use Benchmark;

$opt_loop_count=1000; # Change this to make test harder/easier
$opt_field_count=1000;

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

$opt_field_count=min($opt_field_count,$limits->{'max_columns'},
		     ($limits->{'query_size'}-30)/14);

$opt_loop_count*=10 if ($opt_field_count<100);	# mSQL has so few fields...

if ($opt_small_test)
{
  $opt_loop_count/=10;
  $opt_field_count/=10;
}


print "Testing of some unusual tables\n";
print "All tests are done $opt_loop_count times with $opt_field_count fields\n\n";


####
####  Testing many fields
####

$dbh = $server->connect();
print "Testing table with $opt_field_count fields\n";

$sth = $dbh->do("drop table bench1");

my @fields=();
my @index=();
my $fields="i1";
push(@fields,"$fields int");
$values= "1," x ($opt_field_count-1) . "1";
for ($i=2 ; $i <= $opt_field_count ; $i++)
{
  push(@fields,"i${i} int");
  $fields.=",i${i}";
}

$start_time=new Benchmark;

do_many($dbh,$server->create("bench1",\@fields,\@index));
$sth = $dbh->do("insert into bench1 values ($values)") or die $DBI::errstr;

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

test_query("Testing select * from table with 1 record",
	   "Time to select_many_fields",
	   "select * from bench1",
	   $dbh,$opt_loop_count);

test_query("Testing select all_fields from table with 1 record",
	   "Time to select_many_fields",
	   "select $fields from bench1",
	   $dbh,$opt_loop_count);

test_query("Testing insert VALUES()",
	   "Time to insert_many_fields",
	   "insert into bench1 values($values)",
	   $dbh,$opt_loop_count);

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

test_command("Testing insert (all_fields) VALUES()",
	     "Time to insert_many_fields",
	     "insert into bench1 ($fields) values($values)",
	     $dbh,$opt_loop_count);

$sth = $dbh->do("drop table bench1") or die $DBI::errstr;

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

################################ END ###################################
####
#### End of the test...Finally print time used to execute the
#### whole test.

$dbh->disconnect;

end_benchmark($start_time);


############################ HELP FUNCTIONS ##############################

sub test_query
{
  my($test_text,$result_text,$query,$dbh,$count)=@_;
  my($i,$loop_time,$end_time);

  print $test_text . "\n";
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $count ; $i++)
  {
    defined(fetch_all_rows($dbh,$query)) or die $DBI::errstr;
  }
  $end_time=new Benchmark;
  print $result_text . "($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}


sub test_command
{
  my($test_text,$result_text,$query,$dbh,$count)=@_;
  my($i,$loop_time,$end_time);

  print $test_text . "\n";
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $count ; $i++)
  {
    $dbh->do($query) or die $DBI::errstr;
  }
  $end_time=new Benchmark;
  print $result_text . "($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}
