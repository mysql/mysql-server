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
# Test of creating the ATIS database and doing many different selects on it
#
# changes made for Oracle compatibility
# - added Oracle to the '' to ' ' translation
# - skip blank lines from the datafiles
# - skip a couple of the tests in Q4 that Oracle doesn't understand
################### Standard benchmark inits ##############################

use DBI;
use Benchmark;

$opt_loop_count=100;		# Run selects this many times

chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

if ($opt_small_test)
{
  $opt_loop_count/=10;
}

print "ATIS table test\n\n";

####
####  Connect and start timeing
####

$dbh = $server->connect();
$start_time=new Benchmark;

####
#### Create needed tables
####

init_data();			# Get table definitions

if (!$opt_skip_create)
{
  print "Creating tables\n";
  $loop_time= new Benchmark;
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    my $array_ref = $tables[$ti];

    # This may fail if we have no table so do not check answer
    $sth = $dbh->do("drop table $table_name" . $server->{'drop_attr'});

    print "Creating table $table_name\n" if ($opt_verbose);
    do_many($dbh,@$array_ref);
  }
  $end_time=new Benchmark;
  print "Time for create_table (" . ($#tables+1) ."): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";

  if ($opt_fast && defined($server->{vacuum}))
  {
    $server->vacuum(0,\$dbh);
  }

####
#### Insert data
####

  print "Inserting data\n";

  $loop_time= new Benchmark;
  $row_count=0;
  $double_quotes=$server->{'double_quotes'};

  if ($opt_lock_tables)
  {
    @tmp=@table_names; push(@tmp,@extra_names);
    print "LOCK TABLES @tmp\n" if ($opt_debug);
    $sth = $dbh->do("LOCK TABLES " . join(" WRITE,", @tmp) . " WRITE") ||
      die $DBI::errstr;
  }

  if ($opt_fast && $server->{'limits'}->{'load_data_infile'})
  {
    for ($ti = 0; $ti <= $#table_names; $ti++)
    {
      my $table_name = $table_names[$ti];
      my $file = "$pwd/Data/ATIS/${table_name}.txt";
      print "$table_name - $file\n" if ($opt_debug);
      $row_count += $server->insert_file($table_name,$file,$dbh);
    }
  }
  else
  {
    for ($ti = 0; $ti <= $#table_names; $ti++)
    {
      my $table_name = $table_names[$ti];
      my $array_ref = $tables[$ti];
      my @table = @$array_ref;
      my $insert_start = "insert into $table_name values (";

      open(DATA, "$pwd/Data/ATIS/${table_name}.txt") || die "Can't open text file: $pwd/Data/ATIS/${table_name}.txt\n";
      while(<DATA>)
      {
	chomp;
	next unless ( $_ =~ /\w/ );	# skip blank lines
	my $command = $insert_start . $_ . ")";
        $command =~ s/\'\'/\' \'/g if ($opt_server =~ /empress/i || $opt_server =~ /oracle/i);
        $command =~ s/\\'//g if ($opt_server =~ /informix/i);
	print "$command\n" if ($opt_debug);
        $command =~ s/\\'/\'\'/g if ($double_quotes);

	$sth = $dbh->do($command) or die "Got error: $DBI::errstr when executing '$command'\n";
	$row_count++;
      }
    }
    close(DATA);
  }
  if ($opt_lock_tables)
  {
    $dbh->do("UNLOCK TABLES");
  }
  $end_time=new Benchmark;
  print "Time to insert ($row_count): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh,@table_names);
}

if ($opt_lock_tables)
{
  @tmp=@table_names; push(@tmp,@extra_names);
  $sth = $dbh->do("LOCK TABLES " . join(" READ,", @tmp) . " READ") ||
    die $DBI::errstr;
}
#
# Now the fun begins.  Let's do some simple queries on the result
#
# The query array is defined as:
# query, number of rows in result, 0|1 where 1 means that the query is possible
#

print "Retrieving data\n";
@Q1=("select_simple_join",
     "select city.city_name,state.state_name,city.city_code from city,state where city.city_code='MATL' and city.state_code=state.state_code",1,1,
     "select city.city_name,state.state_name,city.city_code from state,city where city.state_code=state.state_code",11,1,
     "select month_name.month_name,day_name.day_name from month_name,day_name where month_name.month_number=day_name.day_code",7,1,
     "select month_name.month_name,day_name.day_name from month_name,day_name where month_name.month_number=day_name.day_code and day_name.day_code >= 4",4,1,
     "select flight.flight_code,aircraft.aircraft_type from flight,aircraft where flight.aircraft_code=aircraft.aircraft_code",579,1,
     );

@Q2=("select_join",
     "select airline.airline_name,aircraft.aircraft_type from aircraft,airline,flight where flight.aircraft_code=aircraft.aircraft_code and flight.airline_code=airline.airline_code",579,1);

@Q21=("select_key_prefix_join",
     "select fare.fare_code from restrict_carrier,airline,fare where restrict_carrier.airline_code=airline.airline_code and fare.restrict_code=restrict_carrier.restrict_code",5692,1,
    );

@Q3=("select_distinct",
     "select distinct category from aircraft",6,1,
     "select distinct from_airport from flight",9,1,
     "select distinct aircraft_code from flight",22,1,
     "select distinct * from fare",534,1,
     "select distinct flight_code from flight_fare",579,1,
     "select distinct flight.flight_code,aircraft.aircraft_type from flight,aircraft where flight.aircraft_code=aircraft.aircraft_code",579,1,
     "select distinct airline.airline_name,aircraft.aircraft_type from aircraft,airline,flight where flight.aircraft_code=aircraft.aircraft_code and flight.airline_code=airline.airline_code",44,$limits->{'join_optimizer'},
     "select distinct airline.airline_name,aircraft.aircraft_type from flight,aircraft,airline where flight.aircraft_code=aircraft.aircraft_code and flight.airline_code=airline.airline_code",44,1,
     );

@Q4=("select_group",
     "select day_name.day_name,day_name.day_code,count(*) from flight_day,day_name where day_name.day_code=flight_day.day_code group by day_name.day_name,day_name.day_code order by day_name.day_code",7,$limits->{'group_functions'},
     "select day_name.day_name,count(*) from flight_day,day_name where day_name.day_code=flight_day.day_code group by day_name.day_name",7,$limits->{'group_functions'},
     "select month_name,day_name from month_name,day_name where month_number=day_code and day_code>3 group by month_name,day_name",4,$limits->{'group_functions'},
     "select day_name.day_name,flight_day.day_code,count(*) from flight_day,day_name where day_name.day_code=flight_day.day_code group by flight_day.day_code,day_name.day_name order by flight_day.day_code",7,$limits->{'group_functions'},
     "select sum(engines) from aircraft",1,$limits->{'group_functions'},
     "select avg(engines) from aircraft",1,$limits->{'group_functions'},
     "select avg(engines) from aircraft where engines>0",1,$limits->{'group_functions'},
     "select count(*),min(pay_load),max(pay_load) from aircraft where pay_load>0",1,$limits->{'group_functions'},
     "select min(flight_code),min(flight_code) from flight",1,$limits->{'group_functions'},
     "select min(from_airport),min(to_airport) from flight",1,$limits->{'group_functions'} && $limits->{'group_func_sql_min_str'},
     "select count(*) from aircraft where pay_load>10000",1,$limits->{'group_functions'},
     "select count(*) from aircraft where pay_load<>0",1,$limits->{'group_functions'},
     "select count(*) from flight where flight_code >= 112793",1,$limits->{'group_functions'},
     "select count(if(pay_load,1,NULL)) from aircraft",1,$limits->{'if'} && $limits->{'group_functions'},
     "select std(engines) from aircraft",1,$limits->{'group_func_extra_std'},
     "SELECT from_airport,to_airport,avg(time_elapsed) FROM flight WHERE from_airport='ATL' AND to_airport='BOS' group by from_airport,to_airport",1,$limits->{'group_functions'},
     "select city_code, avg(ground_fare) from ground_service where ground_fare<>0 group by city_code",11,$limits->{'group_functions'},
     "select count(*), ground_service.city_code from ground_service group by ground_service.city_code",12,$limits->{'group_functions'},
     "select category,count(*) as totalnr from aircraft where engines=2 group by category having totalnr>4",3,$limits->{'group_functions'} && $limits->{'having_with_alias'},
     "select category,count(*) from aircraft where engines=2 group by category having count(*)>4",3,$limits->{'group_functions'} && $limits->{'having_with_group'},
     "select flight_number,range_miles,fare_class FROM aircraft,flight,flight_class WHERE flight.flight_code=flight_class.flight_code AND flight.aircraft_code=aircraft.aircraft_code AND range_miles<>0 AND (stops=1 OR stops=2) GROUP BY flight_number,range_miles,fare_class",150,$limits->{'group_functions'},
     "select distinct from_airport.time_zone_code,to_airport.time_zone_code,(FLOOR(arrival_time/100)*60+MOD(arrival_time,100)-FLOOR(departure_time/100)*60-MOD(departure_time,100)-time_elapsed)/60 AS time_zone_diff FROM flight,airport AS from_airport,airport AS to_airport WHERE flight.from_airport=from_airport.airport_code AND flight.to_airport=to_airport.airport_code GROUP BY from_airport.time_zone_code,to_airport.time_zone_code,arrival_time,departure_time,time_elapsed",21,$limits->{'func_odbc_mod'} && $limits->{'func_odbc_floor'} && $limits->{'group_functions'},
     "select DISTINCT from_airport.time_zone_code,to_airport.time_zone_code,MOD((FLOOR(arrival_time/100)*60+MOD(arrival_time,100)-FLOOR(departure_time/100)*60-MOD(departure_time,100)-time_elapsed)/60+36,24)-12 AS time_zone_diff FROM flight,airport AS from_airport,airport AS to_airport WHERE flight.from_airport=from_airport.airport_code AND flight.to_airport=to_airport.airport_code and MOD((FLOOR(arrival_time/100)*60+MOD(arrival_time,100)-FLOOR(departure_time/100)*60-MOD(departure_time,100)-time_elapsed)/60+36,24)-12 < 10",14,$limits->{'func_odbc_mod'} && $limits->{'func_odbc_floor'} && $limits->{'group_functions'},
     "select from_airport,to_airport,range_miles,time_elapsed FROM aircraft,flight WHERE aircraft.aircraft_code=flight.aircraft_code AND to_airport NOT LIKE from_airport AND range_miles<>0 AND time_elapsed<>0 GROUP BY from_airport,to_airport,range_miles,time_elapsed",409,$limits->{'group_functions'} && $limits->{'like_with_column'},
     "SELECT airport.country_name,state.state_name,city.city_name,airport_service.direction FROM airport_service,state,airport,city WHERE airport_service.city_code=city.city_code AND airport_service.airport_code=airport.airport_code AND state.state_code=airport.state_code AND state.state_code=city.state_code AND airport.state_code=city.state_code AND airport.country_name=city.country_name AND airport.country_name=state.country_name AND city.time_zone_code=airport.time_zone_code GROUP BY airport.country_name,state.state_name,city.city_name,airport_service.direction ORDER BY state_name",11,$limits->{'group_functions'},
     "SELECT airport.country_name,state.state_name,city.city_name,airport_service.direction FROM airport_service,state,airport,city WHERE airport_service.city_code=city.city_code AND airport_service.airport_code=airport.airport_code AND state.state_code=airport.state_code AND state.state_code=city.state_code AND airport.state_code=city.state_code AND airport.country_name=city.country_name AND airport.country_name=state.country_name AND city.time_zone_code=airport.time_zone_code GROUP BY airport.country_name,state.state_name,city.city_name,airport_service.direction ORDER BY state_name DESC",11,$limits->{'group_functions'},
     "SELECT airport.country_name,state.state_name,city.city_name,airport_service.direction FROM airport_service,state,airport,city WHERE airport_service.city_code=city.city_code AND airport_service.airport_code=airport.airport_code AND state.state_code=airport.state_code AND state.state_code=city.state_code AND airport.state_code=city.state_code AND airport.country_name=city.country_name AND airport.country_name=state.country_name AND city.time_zone_code=airport.time_zone_code GROUP BY airport.country_name,state.state_name,city.city_name,airport_service.direction ORDER BY state_name",11,$limits->{'group_functions'},
     "SELECT from_airport,to_airport,fare.fare_class,night,one_way_cost,rnd_trip_cost,class_days FROM compound_class,fare WHERE compound_class.fare_class=fare.fare_class AND one_way_cost <= 825 AND one_way_cost >= 280 AND from_airport='SFO' AND to_airport='DFW' GROUP BY from_airport,to_airport,fare.fare_class,night,one_way_cost,rnd_trip_cost,class_days ORDER BY one_way_cost",10,$limits->{'group_functions'},
     "select engines,category,cruising_speed,from_airport,to_airport FROM aircraft,flight WHERE category='JET' AND ENGINES >= 1 AND aircraft.aircraft_code=flight.aircraft_code AND to_airport NOT LIKE from_airport AND stops>0 GROUP BY engines,category,cruising_speed,from_airport,to_airport ORDER BY engines DESC",29,$limits->{'group_functions'} && $limits->{'like_with_column'},
     );

@Q=(\@Q1,\@Q2,\@Q21,\@Q3,\@Q4);


foreach $Q (@Q)
{
  $count=$estimated=0;
  $loop_time= new Benchmark;
  for ($i=1 ; $i <= $opt_loop_count; $i++)
  {
    for ($j=1 ; $j < $#$Q ; $j+=3)
    {
      if ($Q->[$j+2])
      {				# We can do it with current limits
	$count++;
	if ($i == 100)		# Do something different
	{
	  if (($row_count=fetch_all_rows($dbh,$server->query($Q->[$j]))) !=
	      $Q->[$j+1])
	  {
	    if ($row_count == undef())
	    {
	      die "Got error: $DBI::errstr when executing " . $Q->[$j] ."\n"."got $row_count instead of $Q->[$j+1] *** \n";
	    }
	    print "Warning: Query '" . $Q->[$j] . "' returned $row_count rows when it should have returned " . $Q->[$j+1] . " rows\n";
	  }
	}
	else
	{
	  defined(fetch_all_rows($dbh,$server->query($Q->[$j])))
	    or die "ERROR: $DBI::errstr executing '$Q->[$j]'\n";
	}
      }
    }
    $end_time=new Benchmark;
    last if ($estimated=predict_query_time($loop_time,$end_time,\$count,$i,
					   $opt_loop_count));
    print "Loop $i\n" if ($opt_verbose);
  }
  if ($count)
  {
    if ($estimated)
    { print "Estimated time"; }
    else
    { print "Time"; }
    print  " for " . $Q->[0] . " ($count): " .
      timestr(timediff($end_time, $loop_time),"all") . "\n";
  }
}

print "\n";

####
#### Delete the tables
####

if (!$opt_skip_delete)				# Only used when testing
{
  print "Removing tables\n";
  $loop_time= new Benchmark;
  if ($opt_lock_tables)
  {
    $sth = $dbh->do("UNLOCK TABLES") || die $DBI::errstr;
  }
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    $sth = $dbh->do("drop table $table_name" . $server->{'drop_attr'});
  }

  $end_time=new Benchmark;
  print "Time to drop_table (" .($#table_names+1) . "): " .
    timestr(timediff($end_time, $loop_time),"all") . "\n";
}

if ($opt_fast && defined($server->{vacuum}))
{
  $server->vacuum(0,\$dbh);
}

####
#### End of benchmark
####

$dbh->disconnect;				# close connection

end_benchmark($start_time);


sub init_data
{
  @aircraft=
    $server->create("aircraft",
		    ["aircraft_code char(3) NOT NULL",
		     "aircraft_type char(64) NOT NULL",
		     "engines tinyint(1) NOT NULL",
		     "category char(10) NOT NULL",
		     "wide_body char(3) NOT NULL",
		     "wing_span float(6,2) NOT NULL",
		     "length1 float(6,2) NOT NULL",
		     "weight integer(7) NOT NULL",
		     "capacity smallint(3) NOT NULL",
		     "pay_load integer(7) NOT NULL",
		     "cruising_speed mediumint(5) NOT NULL",
		     "range_miles mediumint(5) NOT NULL",
		     "pressurized char(3) NOT NULL"],
		    ["PRIMARY KEY (aircraft_code)"]);
  @airline=
    $server->create("airline",
		    ["airline_code char(2) NOT NULL",
		     "airline_name char(64) NOT NULL",
		     "notes char(38) NOT NULL"],
		    ["PRIMARY KEY (airline_code)"]);
  @airport=
    $server->create("airport",
		    ["airport_code char(3) NOT NULL",
		     "airport_name char(40) NOT NULL",
		     "location char(36) NOT NULL",
		     "state_code char(2) NOT NULL",
		     "country_name char(25) NOT NULL",
		     "time_zone_code char(3) NOT NULL"],
		    ["PRIMARY KEY (airport_code)"]);
  @airport_service=
    $server->create("airport_service",
		    ["city_code char(4) NOT NULL",
		     "airport_code char(3) NOT NULL",
		     "miles_distant float(4,1) NOT NULL",
		     "direction char(3) NOT NULL",
		     "minutes_distant smallint(3) NOT NULL"],
		    ["PRIMARY KEY (city_code, airport_code)"]);
  @city=
    $server->create("city",
		    ["city_code char(4) NOT NULL",
		     "city_name char(25) NOT NULL",
		     "state_code char(2) NOT NULL",
		     "country_name char(25) NOT NULL",
		     "time_zone_code char(3) NOT NULL"],
		    ["PRIMARY KEY (city_code)"]);
  @class_of_service=
    $server->create("class_of_service",
		    ["class_code char(2) NOT NULL",
		     "rank tinyint(2) NOT NULL",
		     "class_description char(80) NOT NULL"],
		    ["PRIMARY KEY (class_code)"]);
  @code_description=
    $server->create("code_description",
		    ["code char(5) NOT NULL",
		     "description char(110) NOT NULL"],
		    ["PRIMARY KEY (code)"]);
  @compound_class=
    $server->create("compound_class",
		    ["fare_class char(3) NOT NULL",
		     "base_class char(2) NOT NULL",
		     "class_type char(10) NOT NULL",
		     "premium char(3) NOT NULL",
		     "economy char(3) NOT NULL",
		     "discounted char(3) NOT NULL",
		     "night char(3) NOT NULL",
		     "season_fare char(4) NOT NULL",
		     "class_days char(7) NOT NULL"],
		    ["PRIMARY KEY (fare_class)"]);
  @connect_leg=
    $server->create("connect_leg",
		    ["connect_code integer(8) NOT NULL",
		     "leg_number tinyint(1) NOT NULL",
		     "flight_code integer(8) NOT NULL"],
		    ["PRIMARY KEY (connect_code, leg_number, flight_code)"]);
  @connection=
    $server->create("fconnection",
		    ["connect_code integer(8) NOT NULL",
		     "from_airport char(3) NOT NULL",
		     "to_airport char(3) NOT NULL",
		     "departure_time smallint(4) NOT NULL",
		     "arrival_time smallint(4) NOT NULL",
		     "flight_days char(7) NOT NULL",
		     "stops tinyint(1) NOT NULL",
		     "connections tinyint(1) NOT NULL",
		     "time_elapsed smallint(4) NOT NULL"],
		    ["PRIMARY KEY (connect_code)",
		     "INDEX from_airport1 (from_airport)",
		     "INDEX to_airport1 (to_airport)"]);
  @day_name=
    $server->create("day_name",
		    ["day_code tinyint(1) NOT NULL",
		     "day_name char(9) NOT NULL"],
		    ["PRIMARY KEY (day_code)"]);
  @dual_carrier=
    $server->create("dual_carrier",
		    ["main_airline char(2) NOT NULL",
		     "dual_airline char(2) NOT NULL",
		     "low_flight smallint(4) NOT NULL",
		     "high_flight smallint(4) NOT NULL",
		     "fconnection_name char(64) NOT NULL"],
		    ["PRIMARY KEY (main_airline, dual_airline, low_flight)",
		     "INDEX main_airline1 (main_airline)"]);

  @fare=
    $server->create("fare",
		    ["fare_code char(8) NOT NULL",
		     "from_airport char(3) NOT NULL",
		     "to_airport char(3) NOT NULL",
		     "fare_class char(3) NOT NULL",
		     "fare_airline char(2) NOT NULL",
		     "restrict_code char(5) NOT NULL",
		     "one_way_cost float(7,2) NOT NULL",
		     "rnd_trip_cost float(8,2) NOT NULL"],
		    ["PRIMARY KEY (fare_code)",
		     "INDEX from_airport2 (from_airport)",
		     "INDEX to_airport2 (to_airport)"]);
  @flight=
    $server->create("flight",
		    ["flight_code integer(8) NOT NULL",
		     "flight_days char(7) NOT NULL",
		     "from_airport char(3) NOT NULL",
		     "to_airport char(3) NOT NULL",
		     "departure_time smallint(4) NOT NULL",
		     "arrival_time smallint(4) NOT NULL",
		     "airline_code char(2) NOT NULL",
		     "flight_number smallint(4) NOT NULL",
		     "class_string char(8) NOT NULL",
		     "aircraft_code char(3) NOT NULL",
		     "meal_code char(7) NOT NULL",
		     "stops tinyint(1) NOT NULL",
		     "dual_carrier char(1) NOT NULL",
		     "time_elapsed smallint(4) NOT NULL"],
		    ["PRIMARY KEY (flight_code)",
		     "INDEX from_airport3 (from_airport)",
		     "INDEX to_airport3 (to_airport)"]);
  @flight_class=
    $server->create("flight_class",
		    ["flight_code integer(8) NOT NULL",
		     "fare_class char(3) NOT NULL"],
		    ["PRIMARY KEY (flight_code, fare_class)"]);
  @flight_day=
    $server->create("flight_day",
		    ["day_mask char(7) NOT NULL",
		     "day_code tinyint(1) NOT NULL",
		     "day_name char(9) NOT NULL"],
		    ["PRIMARY KEY (day_mask, day_code)"]);
  @flight_fare=
    $server->create("flight_fare",
		    ["flight_code integer(8) NOT NULL",
		     "fare_code char(8) NOT NULL"],
		    ["PRIMARY KEY (flight_code, fare_code)"]);
  @food_service=
    $server->create("food_service",
		    ["meal_code char(4) NOT NULL",
		     "meal_number tinyint(1) NOT NULL",
		     "meal_class char(10) NOT NULL",
		     "meal_description char(10) NOT NULL"],
		    ["PRIMARY KEY (meal_code, meal_number, meal_class)"]);
  @ground_service=
    $server->create("ground_service",
		    ["city_code char(4) NOT NULL",
		     "airport_code char(3) NOT NULL",
		     "transport_code char(1) NOT NULL",
		     "ground_fare float(6,2) NOT NULL"],
		    ["PRIMARY KEY (city_code, airport_code, transport_code)"]);
  @time_interval=
    $server->create("time_interval",
		    ["period char(20) NOT NULL",
		     "begin_time smallint(4) NOT NULL",
		     "end_time smallint(4) NOT NULL"],
		    ["PRIMARY KEY (period, begin_time)"]);
  @month_name=
    $server->create("month_name",
		    ["month_number tinyint(2) NOT NULL",
		     "month_name char(9) NOT NULL"],
		    ["PRIMARY KEY (month_number)"]);
  @restrict_carrier=
    $server->create("restrict_carrier",
		    ["restrict_code char(5) NOT NULL",
		     "airline_code char(2) NOT NULL"],
		    ["PRIMARY KEY (restrict_code, airline_code)"]);
  @restrict_class=
    $server->create("restrict_class",
		    ["restrict_code char(5) NOT NULL",
		     "ex_fare_class char(12) NOT NULL"],
		    ["PRIMARY KEY (restrict_code, ex_fare_class)"]);
  @restriction=
    $server->create("restriction",
		    ["restrict_code char(5) NOT NULL",
		     "application char(80) NOT NULL",
		     "no_discounts char(80) NOT NULL",
		     "reserve_ticket smallint(3) NOT NULL",
		     "stopovers char(1) NOT NULL",
		     "return_min smallint(3) NOT NULL",
		     "return_max smallint(3) NOT NULL"],
		    ["PRIMARY KEY (restrict_code)"]);
  @state=
    $server->create("state",
		    ["state_code char(2) NOT NULL",
		     "state_name char(25) NOT NULL",
		     "country_name char(25) NOT NULL"],
		    ["PRIMARY KEY (state_code)"]);
  @stop=
    $server->create("stop1",
		    ["flight_code integer(8) NOT NULL",
		     "stop_number tinyint(1) NOT NULL",
		     "stop_flight integer(8) NOT NULL"],
		    ["PRIMARY KEY (flight_code, stop_number)"]);
  @time_zone=
    $server->create("time_zone",
		    ["time_zone_code char(3) NOT NULL",
		     "time_zone_name char(32) NOT NULL"],
		    ["PRIMARY KEY (time_zone_code, time_zone_name)"]);
  @transport=
    $server->create("transport",
		    ["transport_code char(1) NOT NULL",
		     "transport_desc char(32) NOT NULL"],
		    ["PRIMARY KEY (transport_code)"]);

# Avoid not used warnings

  @tables =
    (\@aircraft, \@airline, \@airport, \@airport_service,
     \@city, \@class_of_service, \@code_description,
     \@compound_class, \@connect_leg, \@connection, \@day_name,
     \@dual_carrier, \@fare, \@flight, \@flight_class, \@flight_day,
     \@flight_fare, \@food_service, \@ground_service, \@time_interval,
     \@month_name,
     \@restrict_carrier, \@restrict_class, \@restriction, \@state, \@stop,
     \@time_zone, \@transport);

  @table_names =
    ("aircraft", "airline", "airport", "airport_service",
     "city", "class_of_service", "code_description",
     "compound_class", "connect_leg", "fconnection", "day_name",
     "dual_carrier", "fare", "flight", "flight_class", "flight_day",
     "flight_fare", "food_service", "ground_service", "time_interval",
     "month_name",
     "restrict_carrier", "restrict_class", "restriction", "state", "stop1",
     "time_zone", "transport");

# Alias used in joins
  @extra_names=("airport as from_airport","airport as to_airport");
}
