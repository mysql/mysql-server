#!@PERL@
# Test of table elimination feature

use Cwd;
use DBI;
use Getopt::Long;
use Benchmark;

$opt_loop_count=100000;
$opt_medium_loop_count=10000;
$opt_small_loop_count=100;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

if ($opt_small_test)
{
  $opt_loop_count/=10;
  $opt_medium_loop_count/=10;
  $opt_small_loop_count/=10;
}

print "Testing table elimination feature\n";
print "The test table has $opt_loop_count rows.\n\n";

# A query to get the recent versions of all attributes:
$select_current_full_facts="
  select 
    F.id, A1.attr1, A2.attr2
  from 
    elim_facts F 
    left join elim_attr1 A1 on A1.id=F.id
    left join elim_attr2 A2 on A2.id=F.id and 
                               A2.fromdate=(select MAX(fromdate) from
                                            elim_attr2 where id=A2.id);
";
$select_current_full_facts="
  select 
    F.id, A1.attr1, A2.attr2
  from 
    elim_facts F 
    left join elim_attr1 A1 on A1.id=F.id
    left join elim_attr2 A2 on A2.id=F.id and 
                               A2.fromdate=(select MAX(fromdate) from
                                            elim_attr2 where id=F.id);
";
# TODO: same as above but for some given date also? 
# TODO: 


####
####  Connect and start timeing
####

$dbh = $server->connect();
$start_time=new Benchmark;

####
#### Create needed tables
####

goto select_test if ($opt_skip_create);

print "Creating tables\n";
$dbh->do("drop table elim_facts" . $server->{'drop_attr'});
$dbh->do("drop table elim_attr1" . $server->{'drop_attr'});
$dbh->do("drop table elim_attr2" . $server->{'drop_attr'});

# The facts table
do_many($dbh,$server->create("elim_facts",
			     ["id integer"],
			     ["primary key (id)"]));

# Attribute1, non-versioned
do_many($dbh,$server->create("elim_attr1",
			     ["id integer",
                              "attr1 integer"],
			     ["primary key (id)",
                              "key (attr1)"]));

# Attribute2, time-versioned
do_many($dbh,$server->create("elim_attr2",
			     ["id integer",
                              "attr2 integer",
                              "fromdate date"],
			     ["primary key (id, fromdate)",
                              "key (attr2,fromdate)"]));

#NOTE: ignoring: if ($limits->{'views'})
$dbh->do("drop view elim_current_facts");
$dbh->do("create view elim_current_facts as $select_current_full_facts");

if ($opt_lock_tables)
{
  do_query($dbh,"LOCK TABLES elim_current_facts WRITE, elim_facts WRITE, elim_attr1 WRITE, elim_attr2 WRITE");
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh);
}

####
#### Fill the facts table
####
$n_facts= $opt_loop_count;

if ($opt_fast && $server->{transactions})
{
  $dbh->{AutoCommit} = 0;
}

print "Inserting $n_facts rows into facts table\n";
$loop_time=new Benchmark;

$query="insert into elim_facts values (";
for ($id=0; $id < $n_facts ; $id++)
{
  do_query($dbh,"$query $id)");
}

if ($opt_fast && $server->{transactions})
{
  $dbh->commit;
  $dbh->{AutoCommit} = 1;
}

$end_time=new Benchmark;
print "Time to insert ($n_facts): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
#### Fill attr1 table
####
if ($opt_fast && $server->{transactions})
{
  $dbh->{AutoCommit} = 0;
}

print "Inserting $n_facts rows into attr1 table\n";
$loop_time=new Benchmark;

$query="insert into elim_attr1 values (";
for ($id=0; $id < $n_facts ; $id++)
{
  $attr1= ceil(rand($n_facts));
  do_query($dbh,"$query $id, $attr1)");
}

if ($opt_fast && $server->{transactions})
{
  $dbh->commit;
  $dbh->{AutoCommit} = 1;
}

$end_time=new Benchmark;
print "Time to insert ($n_facts): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
#### Fill attr2 table
####
if ($opt_fast && $server->{transactions})
{
  $dbh->{AutoCommit} = 0;
}

print "Inserting $n_facts rows into attr2 table\n";
$loop_time=new Benchmark;

for ($id=0; $id < $n_facts ; $id++)
{
  # Two values for each $id - current one and obsolete one.
  $attr1= ceil(rand($n_facts));
  $query="insert into elim_attr2 values ($id, $attr1, now())";
  do_query($dbh,$query);
  $query="insert into elim_attr2 values ($id, $attr1, '2009-01-01')";
  do_query($dbh,$query);
}

if ($opt_fast && $server->{transactions})
{
  $dbh->commit;
  $dbh->{AutoCommit} = 1;
}

$end_time=new Benchmark;
print "Time to insert ($n_facts): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

####
####  Finalize the database population
####

if ($opt_lock_tables)
{
  do_query($dbh,"UNLOCK TABLES");
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(1,\$dbh,"elim_facts");
  $server->vacuum(1,\$dbh,"elim_attr1");
  $server->vacuum(1,\$dbh,"elim_attr2");
}

if ($opt_lock_tables)
{
  do_query($dbh,"LOCK TABLES elim_current_facts READ, elim_facts READ, elim_attr1 READ, elim_attr2 READ");
}

####
#### Do some selects on the table
####

select_test:

#
# The selects will be:
#   - N pk-lookups with all attributes 
#   - pk-attribute-based lookup
#   - latest-attribute value based lookup.


###
### Bare facts select:
###
print "testing bare facts facts table\n";
$loop_time=new Benchmark;
$rows=0;
for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
{
  $val= ceil(rand($n_facts));
  $rows+=fetch_all_rows($dbh,"select * from elim_facts where id=$val");
}
$count=$i;

$end_time=new Benchmark;
print "time for select_bare_facts ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";


###
### Full facts select, no elimination:
###
print "testing full facts facts table\n";
$loop_time=new Benchmark;
$rows=0;
for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
{
  $val= rand($n_facts);
  $rows+=fetch_all_rows($dbh,"select * from elim_current_facts where id=$val");
}
$count=$i;

$end_time=new Benchmark;
print "time for select_two_attributes ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

###
### Now with elimination: select only only one fact
###
print "testing selection of one attribute\n";
$loop_time=new Benchmark;
$rows=0;
for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
{
  $val= rand($n_facts);
  $rows+=fetch_all_rows($dbh,"select id, attr1 from elim_current_facts where id=$val");
}
$count=$i;

$end_time=new Benchmark;
print "time for select_one_attribute ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";

###
### Now with elimination: select only only one fact
###
print "testing selection of one attribute\n";
$loop_time=new Benchmark;
$rows=0;
for ($i=0 ; $i < $opt_medium_loop_count ; $i++)
{
  $val= rand($n_facts);
  $rows+=fetch_all_rows($dbh,"select id, attr2 from elim_current_facts where id=$val");
}
$count=$i;

$end_time=new Benchmark;
print "time for select_one_attribute ($count:$rows): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";


;

####
#### End of benchmark
####

if ($opt_lock_tables)
{
  do_query($dbh,"UNLOCK TABLES");
}
if (!$opt_skip_delete)
{
  do_query($dbh,"drop table elim_facts, elim_attr1, elim_attr2" . $server->{'drop_attr'});
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

$dbh->disconnect;				# close connection

end_benchmark($start_time);

