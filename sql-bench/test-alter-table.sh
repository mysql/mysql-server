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
# Test of alter table
#
##################### Standard benchmark inits ##############################

use DBI;
use Benchmark;

$opt_start_field_count=8;	# start with this many fields
$opt_loop_count=20;		# How many tests to do
$opt_row_count=1000; 		# Rows in the table
$opt_field_count=1000;		# Add until this many fields.

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

$opt_field_count=min($opt_field_count,$limits->{'max_columns'},
		     ($limits->{'query_size'}-30)/14);
$opt_start_field_count=min($opt_start_field_count,$limits->{'max_index'});

if ($opt_small_test)
{
  $opt_row_count/=10;
  $opt_field_count/=10;
}

if (!$limits->{'alter_table'})
{
  print("Some of the servers given with --cmp or --server doesn't support ALTER TABLE\nTest aborted\n\n");
  $start_time=new Benchmark;
  end_benchmark($start_time);
  exit 0;
}

print "Testing of ALTER TABLE\n";
print "Testing with $opt_field_count columns and $opt_row_count rows in $opt_loop_count steps\n";

####
#### Create a table and fill it with data
####

$dbh = $server->connect();
@fields=();
@index=();
push(@fields,"i1 int not null");
for ($i=2 ; $i <= $opt_start_field_count ; $i++)
{
  push(@fields,"i${i} int not null");
}
$field_count= $opt_start_field_count;

$start_time=new Benchmark;

$dbh->do("drop table bench");
do_many($dbh,$server->create("bench",\@fields,\@index));

print "Insert data into the table\n";

$loop_time=new Benchmark;
for ($i=0 ; $i < $opt_row_count ; $i++)
{
  $query="insert into bench values ( " . ("$i," x ($opt_start_field_count-1)) . "$i)";
  $dbh->do($query) or die $DBI::errstr;
}
$end_time=new Benchmark;

print "Time for insert ($opt_row_count)",
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";


####
#### Add fields to the table.
####

$loop_time=new Benchmark;
$add= int(($opt_field_count-$opt_start_field_count)/$opt_loop_count)+1;


$add=1 if (!$limits{'alter_add_multi_col'});
$multi_add=$server->{'limits'}->{'alter_add_multi_col'} == 1;

$count=0;
while ($field_count < $opt_field_count)
{
  $count++;
  $end=min($field_count+$add,$opt_field_count);
  $fields="";
  $tmp="ADD ";
  while ($field_count < $end)
  {
    $field_count++;
    $fields.=",$tmp i${field_count} integer";
    $tmp="" if (!$multi_add);			# Adabas
  }
  do_query($dbh,"ALTER TABLE bench " . substr($fields,1));
}

$end_time=new Benchmark;
print "Time for alter_table_add ($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
#### Test adding and deleting index on the first $opt_start_fields
####

$loop_time=new Benchmark;

for ($i=1; $i < $opt_start_field_count ; $i++)
{
  $dbh->do("CREATE INDEX bench_ind$i ON bench (i${i})") || die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for create_index ($opt_start_field_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

$loop_time=new Benchmark;
for ($i=1; $i < $opt_start_field_count ; $i++)
{
  $dbh->do($server->drop_index("bench","bench_ind$i")) || die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for drop_index ($opt_start_field_count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
#### Delete fields from the table
####

goto skip_dropcol if (!$limits->{'alter_table_dropcol'});

$loop_time=new Benchmark;

$count=0;
while ($field_count > $opt_start_field_count)
{
  $count++;
  $end=max($field_count-$add,$opt_start_field_count);
  $fields="";
  while(--$field_count >= $end)
  {
    $fields.=",DROP i${field_count}";
  }
  $dbh->do("ALTER TABLE bench " . substr($fields,1)) || die $DBI::errstr;
}

$end_time=new Benchmark;
print "Time for alter_table_drop ($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";

skip_dropcol:

################################ END ###################################
####
#### End of the test...Finally print time used to execute the
#### whole test.

$dbh->do("drop table bench");

$dbh->disconnect;

end_benchmark($start_time);
