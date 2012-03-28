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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA
#
# AS3AP single-user benchmark.
#

##################### Standard benchmark inits ##############################

use Cwd;
use DBI;
use Benchmark;

$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/bench-init.pl" || die "Can't read Configuration file: $!\n";

$opt_loop_count=1;

#Create tables 

$dbh = $server->connect();

#Create Table
$sth = $dbh->do("drop table uniques");
$sth = $dbh->do("drop table updates");
$sth = $dbh->do("drop table hundred");
$sth = $dbh->do("drop table tenpct");
$sth = $dbh->do("drop table tiny");

#Temporary table
$sth = $dbh->do("drop table saveupdates");

@fields=("col_key     int             not null",
	 "col_int     int             not null",
	 "col_signed  int             not null",
	 "col_float   float           not null",
	 "col_double  float           not null",
	 "col_decim   numeric(18,2)   not null",
	 "col_date    char(20)        not null",
	 "col_code    char(10)        not null",
	 "col_name    char(20)        not null",
	 "col_address varchar(80)     not null");

do_many($dbh,$server->create("uniques",\@fields,[]));
do_many($dbh,$server->create("updates",\@fields,[]));
do_many($dbh,$server->create("hundred",\@fields,[]));
do_many($dbh,$server->create("tenpct",\@fields,[]));
do_many($dbh,$server->create("tiny",["col_key int not null"],[]));

print "Start AS3AP benchmark\n\n";

$start_time=new Benchmark;

print "Load DATA\n";
#Load DATA

@table_names=("uniques","updates","hundred","tenpct","tiny");

$loop_time=new Benchmark;

if ($opt_fast && $server->{'limits'}->{'load_data_infile'})
{
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    my $file = "$pwd/Data/AS3AP/${table_name}\.new";
    print "$table_name - $file\n" if ($opt_debug);
    $row_count += $server->insert_file($table_name,$file,$dbh);
  }
}
else
{
  for ($ti = 0; $ti <= $#table_names; $ti++)
  {
    my $table_name = $table_names[$ti];
    print "$table_name - $file\n" if ($opt_debug);
    my $insert_start = "insert into $table_name values (";
    open(DATA, "$pwd/Data/AS3AP/${table_name}\.new") || die "Can't open text file: $pwd/Data/AS3AP/${table_name}\.new\n";
    while(<DATA>)
    {
      chomp;
      next unless ( $_ =~ /\w/ );     # skip blank lines
      $command = $insert_start."$_".")";
      $command =~ $server->fix_to_insert($command);
      print "$command\n" if ($opt_debug);
      $sth = $dbh->do($command) or die "Got error: $DBI::errstr when executing '$command'\n";
	  $row_count++;
    }
    close(DATA);
  }
}

$end_time=new Benchmark;
print "Time for Load Data - " . "($row_count): " .
timestr(timediff($end_time, $loop_time),"all") . "\n\n";


print "Create Index\n";

test_command("create_idx_uniques_key_bt",
	     "time for create_idx_uniques_key_bt",
	     "create unique index uniques_key_bt on uniques (col_key)",$dbh,$opt_loop_count);

test_command("create_idx_updates_key_bt",
	     "time for create_idx_updates_key_bt",
	     "create unique index updates_key_bt on updates (col_key)",$dbh,$opt_loop_count);

test_command("create_idx_hundred_key_bt",
	     "time for create_idx_hundred_key_bt",
	     "create unique index hundred_key_bt on hundred (col_key)",
	     $dbh,$opt_loop_count);

test_command("create_idx_tenpct_key_bt",
	     "time for create_idx_tenpct_key_bt",
	     "create unique index tenpct_key_bt on tenpct (col_key)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_key_code_bt",
	     "time for create_idx_tenpct_key_code_bt",
	     "create index tenpct_key_code_bt on tenpct (col_key,col_code)",
	     $dbh,$opt_loop_count);

test_command("create_idx_tiny_key_bt",
	     "time for create_idx_tiny_key_bt",
	     "create index tiny_key_bt on tiny (col_key)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_int_bt",
	     "time for create_idx_tenpct_int_bt",
	     "create index tenpct_int_bt on tenpct (col_int)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_signed_bt",
	     "time for create_idx_tenpct_signed_bt",
	     "create index tenpct_signed_bt on tenpct (col_signed)",$dbh,$opt_loop_count);

test_command("create_idx_uniques_code_h",
	     "time for create_idx_uniques_code_h",
	     "create index uniques_code_h on uniques (col_code)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_double_bt",
	     "time for create_idx_tenpct_double_bt",
	     "create index tenpct_double_bt on tenpct (col_double)",$dbh,$opt_loop_count);


test_command("create_idx_updates_decim_bt",
	     "time for create_idx_updates_decim_bt",
	     "create index updates_decim_bt on updates (col_decim)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_float_bt",
	     "time for create_idx_tenpct_float_bt",
	     "create index tenpct_float_bt on tenpct (col_float)",$dbh,$opt_loop_count);

test_command("create_idx_updates_int_bt",
	     "time for create_idx_updates_int_bt",
	     "create index updates_int_bt on updates (col_int)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_decim_bt",
	     "time for create_idx_tenpct_decim_bt",
	     "create index tenpct_decim_bt on tenpct (col_decim)",$dbh,$opt_loop_count);

test_command("create_idx_hundred_code_h",
	     "time for create_idx_hundred_code_h",
	     "create index hundred_code_h on hundred (col_code)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_name_h",
	     "time for create_idx_tenpct_name_h",
	     "create index tenpct_name_h on tenpct (col_name)",$dbh,$opt_loop_count);

test_command("create_idx_updates_code_h",
	     "time for create_idx_updates_code_h",
	     "create index updates_code_h on updates (col_code)",$dbh,$opt_loop_count);

test_command("create_idx_tenpct_code_h",
	     "time for create_idx_tenpct_code_h",
	     "create index tenpct_code_h on tenpct (col_code)",$dbh,$opt_loop_count);

test_command("create_idx_updates_double_bt",
	     "time for create_idx_updates_double_bt",
	     "create index updates_double_bt on updates (col_double)",$dbh,$opt_loop_count);

test_command("create_idx_hundred_foreign",
	     "time for create_idx_hundred_foreign",
	     "alter table hundred add constraint fk_hundred_updates foreign key (col_signed) 
				      references updates (col_key)",$dbh,$opt_loop_count);

test_query("sel_1_cl",
	   "Time to sel_1_cl",
	   "select col_key, col_int, col_signed, col_code, col_double, col_name 
 	    from updates where col_key = 1000",$dbh,$opt_loop_count);

test_query("join_3_cl",
	   "Time to join_3_cl",
	   "select uniques.col_signed, uniques.col_date, 
		   hundred.col_signed, hundred.col_date, 
		   tenpct.col_signed, tenpct.col_date 
	    from uniques, hundred, tenpct 
	    where uniques.col_key = hundred.col_key 
		  and uniques.col_key = tenpct.col_key 
		  and uniques.col_key = 1000",$dbh,$opt_loop_count);

test_query("sel_100_ncl",
	   "Time to sel_100_ncl",
	   "select col_key, col_int, col_signed, col_code,col_double, col_name
	    from updates where col_int <= 100",$dbh,$opt_loop_count);

test_query("table_scan",
	   "Time to table_scan",
	   "select * from uniques where col_int = 1",$dbh,$opt_loop_count);

test_query("agg_func",
	   "Time for agg_func",
	   "select min(col_key) from hundred group by col_name",$dbh,$opt_loop_count);

test_query("agg_scal",
	   "Time for agg_scal",
	   "select min(col_key) from uniques",$dbh,$opt_loop_count);

test_query("sel_100_cl",
	  "Time for sel_100_cl",
	  "select col_key, col_int, col_signed, col_code, 
		  col_double, col_name 
	   from updates where col_key <= 100",$dbh,$opt_loop_count);

test_query("join_3_ncl",
	   "Time for join_3_ncl",
	   "select uniques.col_signed, uniques.col_date, 
		   hundred.col_signed, hundred.col_date, 
		   tenpct.col_signed, tenpct.col_date 
	    from uniques, hundred, tenpct 
	    where uniques.col_code = hundred.col_code 
		  and uniques.col_code = tenpct.col_code 
		  and uniques.col_code = 'BENCHMARKS'",$dbh,$opt_loop_count);

test_query("sel_10pct_ncl",
	   "Time for sel_10pct_ncl",
	   "select col_key, col_int, col_signed, col_code, 
		   col_double, col_name 
	    from tenpct 
	    where col_name = 'THE+ASAP+BENCHMARKS+'",$dbh,$opt_loop_count);

if ($limits->{'subqueries'}){
  test_query("agg_simple_report",
	     "Time for agg_simple_report",
	     "select avg(updates.col_decim) 
	      from updates 
	      where updates.col_key in 
			(select updates.col_key 
			 from updates, hundred 
			 where hundred.col_key = updates.col_key 
			       and updates.col_decim > 980000000)",$dbh,$opt_loop_count);
}else{
 print "agg_simple_report - Failed\n\n";
}

test_query("agg_info_retrieval",
	   "Time for agg_info_retrieval",
	   "select count(col_key) 
	    from tenpct 
	    where col_name = 'THE+ASAP+BENCHMARKS' 
		  and col_int <= 100000000 
		  and col_signed between 1 and 99999999 
		  and not (col_float between -450000000 and 450000000) 
		  and col_double > 600000000 
		  and col_decim < -600000000",$dbh,$opt_loop_count);

if ($limits->{'views'}){
  test_query("agg_create_view",
	     "Time for agg_create_view",
	     "create view 
		reportview(col_key,col_signed,col_date,col_decim, 
				col_name,col_code,col_int) as 
			   select updates.col_key, updates.col_signed, 
			   updates.col_date, updates.col_decim, 
			   hundred.col_name, hundred.col_code, 
			   hundred.col_int 
			   from updates, hundred 
			   where updates.col_key = hundred.col_key",$dbh,$opt_loop_count);

  test_query("agg_subtotal_report",
	     "Time for agg_subtotal_report",
	     "select avg(col_signed), min(col_signed), max(col_signed), 
		     max(col_date), min(col_date), 
		     count(distinct col_name), count(col_name), 
		     col_code, col_int 
	      from reportview 
	      where col_decim >980000000 
	      group by col_code, col_int",$dbh,$opt_loop_count);


  test_query("agg_total_report",
	     "Time for agg_total_report",
	     "select avg(col_signed), min(col_signed), max(col_signed), 
		     max(col_date), min(col_date), 
		     count(distinct col_name), count(col_name), 
		     count(col_code), count(col_int) 
	      from reportview 
	      where col_decim >980000000",$dbh,$opt_loop_count);
}else{
  print "agg_create_view - Failed\n\n";
  print "agg_subtotal_report - Failed\n\n";
  print "agg_total_report - Failed\n\n";
}

#fix from here
test_query("join_2_cl",
           "Time for join_2_cl",
           "select uniques.col_signed, uniques.col_name, 
                    hundred.col_signed, hundred.col_name 
             from uniques, hundred 
             where uniques.col_key = hundred.col_key 
              and uniques.col_key =1000"
           ,$dbh,$opt_loop_count);

test_query("join_2",
           "Time for join_2",
           "select uniques.col_signed, uniques.col_name, 
                     hundred.col_signed, hundred.col_name 
                from uniques, hundred 
               where uniques.col_address = hundred.col_address 
                 and uniques.col_address = 'SILICON VALLEY'"
           ,$dbh,$opt_loop_count);

test_query("sel_variable_select_low",
           "Time for sel_variable_select_low",
           "select col_key, col_int, col_signed, col_code, 
                    col_double, col_name 
                    from tenpct 
                    where col_signed < -500000000"
           ,$dbh,$opt_loop_count);

test_query("sel_variable_select_high",
           "Time for sel_variable_select_high",
           "select col_key, col_int, col_signed, col_code,
                    col_double, col_name
                    from tenpct
                    where col_signed < -250000000"
           ,$dbh,$opt_loop_count);

test_query("join_4_cl",
           "Time for join_4_cl",
           "select uniques.col_date, hundred.col_date, 
                    tenpct.col_date, updates.col_date 
             from uniques, hundred, tenpct, updates 
             where uniques.col_key = hundred.col_key 
               and uniques.col_key = tenpct.col_key 
               and uniques.col_key = updates.col_key 
               and uniques.col_key = 1000"
           ,$dbh,$opt_loop_count);

test_query("proj_100",
           "Time for proj_100",
           "select distinct col_address, col_signed from hundred"
           ,$dbh,$opt_loop_count);

test_query("join_4_ncl",
           "Time for join_4_ncl",
           "select uniques.col_date, hundred.col_date, 
                        tenpct.col_date, updates.col_date 
                from uniques, hundred, tenpct, updates 
                where uniques.col_code = hundred.col_code 
                    and uniques.col_code = tenpct.col_code 
                    and uniques.col_code = updates.col_code 
                    and uniques.col_code = 'BENCHMARKS'"
           ,$dbh,$opt_loop_count);

test_query("proj_10pct",
           "Time for proj_10pct",
           "select distinct col_signed from tenpct"
           ,$dbh,$opt_loop_count);

test_query("sel_1_ncl",
           "Time for sel_1_ncl",
           "select col_key, col_int, col_signed, col_code, 
                    col_double, col_name 
                    from updates where col_code = 'BENCHMARKS'"
           ,$dbh,$opt_loop_count);

test_query("join_2_ncl",
           "Time for join_2_ncl",
           "select uniques.col_signed, uniques.col_name, 
                         hundred.col_signed, hundred.col_name 
                    from uniques, hundred 
                    where uniques.col_code = hundred.col_code 
                    and uniques.col_code = 'BENCHMARKS'"
           ,$dbh,$opt_loop_count);

if ($limits->{'foreign_key'}){ 
  do_many($dbh,$server->create("integrity_temp",\@fields,[]));

  test_query("integrity_test_1",
	     "Time for integrity_test",
	     "insert into integrity_temp select * 
	      from hundred where col_int=0",$dbh,$opt_loop_count);

  test_query("integrity_test_2",
	     "Time for integrity_test",
	     "update hundred set col_signed = '-500000000' 
	      where col_int = 0",$dbh,$opt_loop_count);

  test_query("integrity_test_3",
	     "Time for integrity_test",
	     "update hundred set col_signed = '-500000000' 
	      where col_int = 0",$dbh,$opt_loop_count);


}else{
	print "integrity_test  - Failed\n\n";
}

push @drop_seq_command,$server->drop_index("updates","updates_int_bt");
push @drop_seq_command,$server->drop_index("updates","updates_double_bt");
push @drop_seq_command,$server->drop_index("updates","updates_decim_bt");
push @drop_seq_command,$server->drop_index("updates","updates_code_h");

test_many_command("Drop updates keys",
           "Time for drop_updates_keys",
           \@drop_seq_command,$dbh,$opt_loop_count);

do_many($dbh,$server->create("saveupdates",\@fields,[]));
		
test_command("bulk_save",
           "Time for bulk_save",
           "insert into saveupdates select * 
                    from updates where col_key between 5000 and 5999"
           ,$dbh,$opt_loop_count);

test_command("bulk_modify",
           "Time for bulk_modify",
           "update updates 
                    set col_key = col_key - 100000 
                    where col_key between 5000 and 5999"
           ,$dbh,$opt_loop_count);

safe_command("upd_append_duplicate",
           "Time for upd_append_duplicate",
           "insert into updates  
                 values (6000, 0, 60000, 39997.90, 
                          50005.00, 50005.00, 
                          '11/10/1985', 'CONTROLLER', 
                          'ALICE IN WONDERLAND', 
                          'UNIVERSITY OF ILLINOIS AT CHICAGO')"
           ,$dbh,$opt_loop_count);

test_command("upd_remove_duplicate",
           "Time for upd_remove_duplicate",
           "delete from updates where col_key = 6000 and col_int = 0"
           ,$dbh,$opt_loop_count);

test_command("upd_app_t_mid",
           "Time for upd_app_t_mid",
           "insert into updates 
              values (5005, 5005, 50005, 50005.00, 50005.00, 
                      50005.00, '1/1/1988', 'CONTROLLER', 
                      'ALICE IN WONDERLAND', 
                      'UNIVERSITY OF ILLINOIS AT CHICAGO')"
           ,$dbh,$opt_loop_count);

test_command("upd_mod_t_mid",
           "Time for upd_mod_t_mid",
           "update updates set col_key = '-5000' 
                where col_key = 5005"
           ,$dbh,$opt_loop_count);

test_command("upd_del_t_mid",
           "Time for upd_del_t_mid",
           "delete from updates 
               where (col_key='5005') or (col_key='-5000')"
           ,$dbh,$opt_loop_count);

test_command("upd_app_t_end",
           "Time for upd_app_t_end",
           "delete from updates 
               where (col_key='5005') or (col_key='-5000')"
           ,$dbh,$opt_loop_count);

test_command("upd_mod_t_end",
           "Time for upd_mod_t_end",
           "update updates 
                set col_key = -1000 
                where col_key = 1000000001"
           ,$dbh,$opt_loop_count);

test_command("upd_del_t_end",
           "Time for upd_del_t_end",
           "delete from updates where col_key = -1000"
           ,$dbh,$opt_loop_count);

test_command("create_idx_updates_code_h",
	     "time for create_idx_updates_code_h",
	     "create index updates_code_h on updates (col_code)",
	     $dbh,$opt_loop_count);

test_command("upd_app_t_mid",
           "Time for upd_app_t_mid",
           "insert into updates 
              values (5005, 5005, 50005, 50005.00, 50005.00, 
                      50005.00, '1/1/1988', 'CONTROLLER', 
                      'ALICE IN WONDERLAND', 
                      'UNIVERSITY OF ILLINOIS AT CHICAGO')"
           ,$dbh,$opt_loop_count);

test_command("upd_mod_t_cod",
           "Time for upd_mod_t_cod",
           "update updates 
                set col_code = 'SQL+GROUPS' 
                where col_key = 5005"
           ,$dbh,$opt_loop_count);

test_command("upd_del_t_mid",
           "Time for upd_del_t_mid",
           "delete from updates 
               where (col_key='5005') or (col_key='-5000')"
           ,$dbh,$opt_loop_count);

test_command("create_idx_updates_int_bt",
	     "time for create_idx_updates_int_bt",
	     "create index updates_int_bt on updates (col_int)",
	     $dbh,$opt_loop_count);

test_command("upd_app_t_mid",
           "Time for upd_app_t_mid",
           "insert into updates 
              values (5005, 5005, 50005, 50005.00, 50005.00, 
                      50005.00, '1/1/1988', 'CONTROLLER', 
                      'ALICE IN WONDERLAND', 
                      'UNIVERSITY OF ILLINOIS AT CHICAGO')"
           ,$dbh,$opt_loop_count);

test_command("upd_mod_t_int",
           "Time for upd_mod_t_int",
           "update updates set col_int = 50015 where col_key = 5005"
           ,$dbh,$opt_loop_count);

test_command("upd_del_t_mid",
           "Time for upd_del_t_mid",
           "delete from updates 
               where (col_key='5005') or (col_key='-5000')"
           ,$dbh,$opt_loop_count);

test_command("bulk_append",
           "Time for bulk_append",
           "insert into updates select * from saveupdates"
           ,$dbh,$opt_loop_count);

test_command("bulk_delete",
           "Time for bulk_delete",
           "delete from updates where col_key < 0"
           ,$dbh,$opt_loop_count);

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
    defined(fetch_all_rows($dbh,$query)) or warn $DBI::errstr;
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

sub safe_command
{
  my($test_text,$result_text,$query,$dbh,$count)=@_;
  my($i,$loop_time,$end_time);

  print $test_text . "\n";
  $loop_time=new Benchmark;
  for ($i=0 ; $i < $count ; $i++)
  {
    safe_do_many($dbh,$query); 
  }
  $end_time=new Benchmark;
  print $result_text . "($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}

sub test_many_command
{
  my($test_text,$result_text,$query,$dbh,$count)=@_;
  my($i,$loop_time,$end_time);

  $loop_time=new Benchmark;
  for ($i=0 ; $i < $count ; $i++)
  {
    safe_do_many($dbh, @$query);
  }
  $end_time=new Benchmark;
  print $result_text . "($count): " .
  timestr(timediff($end_time, $loop_time),"all") . "\n\n";
}


