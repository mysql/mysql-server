#!/usr/bin/perl
# -*- perl -*-
# Copyright (c) 2000-2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

# Written by Monty for the TCX/Monty Program/Detron benchmark suite.
# Empress and PostgreSQL patches by Luuk de Boer
# Extensions for ANSI SQL and Mimer by Bengt Gunne
# Some additions and corrections by Matthias Urlich
#
# This programs tries to find all limits for a sql server
# It gets the name from what it does to most servers :)
#
# Be sure to use --help before running this!
#
# If you want to add support for another server, add a new package for the
# server in server-cfg.  You only have to support the 'new' and 'version'
# functions. new doesn't need to have any limits if one doesn't want to
# use the benchmarks.
#

# TODO:
# CMT includes types and functions which are synonyms for other types
# and functions, including those in SQL9x. It should label those synonyms
# as such, and clarify ones such as "mediumint" with comments such as
# "3-byte int" or "same as xxx".

$version="1.61";

use Cwd;
use DBI;
use Getopt::Long;
use POSIX;
$pwd = cwd(); $pwd = "." if ($pwd eq '');
require "$pwd/server-cfg" || die "Can't read Configuration file: $!\n";

$opt_server="mysql"; $opt_host="localhost"; $opt_database="test";
$opt_dir="limits";
$opt_user=$opt_password="";$opt_verbose=1;
$opt_debug=$opt_help=$opt_Information=$opt_restart=$opt_force=$opt_quick=0;
$opt_log_all_queries=$opt_fix_limit_file=$opt_batch_mode=$opt_version=0;
$opt_db_start_cmd="";           # the db server start command
$opt_check_server=0;		# Check if server is alive before each query
$opt_sleep=10;                  # time to sleep while starting the db server
$limit_changed=0;               # For configure file
$reconnect_count=0;
$opt_suffix="";
$opt_comment=$opt_config_file=$opt_log_queries_to_file="";
$limits{'crash_me_safe'}='yes';
$prompts{'crash_me_safe'}='crash me safe';
$limits{'operating_system'}= machine();
$prompts{'operating_system'}='crash-me tested on';
$retry_limit=3;

GetOptions("Information","help","server=s","debug","user=s","password=s",
"database=s","restart","force","quick","log-all-queries","comment=s",
"host=s","fix-limit-file","dir=s","db-start-cmd=s","sleep=s","suffix=s",
"batch-mode","config-file=s","log-queries-to-file=s","check-server",
"version",
"verbose!" => \$opt_verbose) || usage();
usage() if ($opt_help || $opt_Information);
version() && exit(0) if ($opt_version);

$opt_suffix = '-'.$opt_suffix if (length($opt_suffix) != 0);
$opt_config_file = "$pwd/$opt_dir/$opt_server$opt_suffix.cfg"
  if (length($opt_config_file) == 0);
$log_prefix='   ###';  # prefix for log lines in result file
$safe_query_log='';
$safe_query_result_log='';
$log{"crash-me"}="";

#!!!

if ($opt_fix_limit_file)
{
  print "Fixing limit file for $opt_server\n";
  read_config_data();
  $limit_changed=1;
  save_all_config_data();
  exit 0;
}

$server=get_server($opt_server,$opt_host,$opt_database);
$opt_server=$server->{'cmp_name'};

$|=1;                           # For debugging

print "Running $0 $version on '",($server_version=$server->version()),"'\n\n";
print "I hope you didn't have anything important running on this server....\n";
read_config_data();
if ($limit_changed)             # Must have been restarted
{
  save_config_data('crash_me_safe','no',"crash me safe");
}

if (!$opt_force && !$opt_batch_mode)
{
  server_info();
}
else
{
  print "Using --force.  I assume you know what you are doing...\n";
}
print "\n";

save_config_data('crash_me_version',$version,"crash me version");
if ($server_version)
{
  save_config_data('server_version',$server_version,"server version");
}
if (length($opt_comment))
{
  save_config_data('user_comment',$opt_comment,"comment");
}

$opt_log=0;
if (length($opt_log_queries_to_file))
{
  open(LOG,">$opt_log_queries_to_file") || 
    die "Can't open file $opt_log_queries_to_file\n";
  $opt_log=1;
}

#
# Set up some limits that's regared as unlimited
# We don't want to take up all resources from the server...
#

$max_connections="+1000";       # Number of simultaneous connections
$max_buffer_size="+16000000";   # size of communication buffer.
$max_string_size="+8000000";    # Enough for this test
$max_name_length="+512";        # Actually 256, but ...
$max_keys="+64";                # Probably too big.
$max_join_tables="+64";         # Probably too big.
$max_columns="+8192";           # Probably too big.
$max_row_length=$max_string_size;
$max_key_length="+8192";        # Big enough
$max_order_by="+64";		# Big enough
$max_expressions="+10000";
$max_big_expressions="+100";
$max_stacked_expressions="+2000";
$query_size=$max_buffer_size;
$longreadlen=16000000;		# For retrieval buffer


#
# First do some checks that needed for the rest of the benchmark
#
use sigtrap;		       # Must be removed with perl5.005_2 on Win98
$SIG{PIPE} = 'IGNORE';
$problem_counter=0;
$SIG{SEGV} = sub {
  $problem_counter +=1;
  if ($problem_counter >= 100) {
    die("Too many problems, try to restart");
  } else {
    warn('SEGFAULT');
  };    
};
$dbh=safe_connect();

#
# Test if the database require RESTRICT/CASCADE after DROP TABLE
#

# Really remove the crash_me table
$prompt="drop table require cascade/restrict";
$drop_attr="";
$dbh->do("drop table crash_me");
$dbh->do("drop table crash_me cascade");
if (!safe_query_l('drop_requires_cascade',
         ["create table crash_me (a integer not null)",
		 "drop table crash_me"]))
{
  $dbh->do("drop table crash_me cascade");  
  if (safe_query_l('drop_requires_cascade',
        ["create table crash_me (a integer not null)",
		  "drop table crash_me cascade"]))
  {
    save_config_data('drop_requires_cascade',"yes","$prompt");
    $drop_attr="cascade";
  }
  else
  {
    die "Can't create and drop table 'crash_me'\n";
  }
}
else
{
  save_config_data('drop_requires_cascade',"no","$prompt");
  $drop_attr="";
}

# Remove tables from old runs
$dbh->do("drop table crash_me $drop_attr");
$dbh->do("drop table crash_me2 $drop_attr");
$dbh->do("drop table crash_me3 $drop_attr");
$dbh->do("drop table crash_q $drop_attr");
$dbh->do("drop table crash_q1 $drop_attr");

$prompt="Tables without primary key";
if (!safe_query_l('no_primary_key',
      ["create table crash_me (a integer not null,b char(10) not null)",
		 "insert into crash_me (a,b) values (1,'a')"]))
{
  if (!safe_query_l('no_primary_key',
      ["create table crash_me (a integer not null,b char(10) not null".
        ", primary key (a))",
	 "insert into crash_me (a,b) values (1,'a')"]))
  {
    die "Can't create table 'crash_me' with one record: $DBI::errstr\n";
  }
  save_config_data('no_primary_key',"no",$prompt);
}
else
{
  save_config_data('no_primary_key',"yes",$prompt);
}

#
#  Define strings for character NULL and numeric NULL used in expressions
#
$char_null=$server->{'char_null'};
$numeric_null=$server->{'numeric_null'};
if ($char_null eq '')
{
  $char_null="NULL";
}
if ($numeric_null eq '')
{
  $numeric_null="NULL";
}

print "$prompt: $limits{'no_primary_key'}\n";

report("SELECT without FROM",'select_without_from',"select 1");
if ($limits{'select_without_from'} ne "yes")
{
  $end_query=" from crash_me";
  $check_connect="select a from crash_me";
}
else
{
  $end_query="";
  $check_connect="select 1";
}

assert($check_connect);
assert("select a from crash_me where b<'b'");

report("Select constants",'select_constants',"select 1 $end_query");
report("Select table_name.*",'table_wildcard',
       "select crash_me.* from crash_me");
report("Allows \' and \" as string markers",'quote_with_"',
       'select a from crash_me where b<"c"');
check_and_report("Double '' as ' in strings",'double_quotes',[],
		 "select 'Walker''s' $end_query",[],"Walker's",1);
check_and_report("Multiple line strings","multi_strings",[],
		 "select a from crash_me where b < 'a'\n'b'",[],"1",0);
check_and_report("\" as identifier quote (ANSI SQL)",'quote_ident_with_"',[],
		 'select "A" from crash_me',[],"1",0);
check_and_report("\` as identifier quote",'quote_ident_with_`',[],
		 'select `A` from crash_me',[],"1",0);
check_and_report("[] as identifier quote",'quote_ident_with_[',[],
		 'select [A] from crash_me',[],"1",0);
report('Double "" in identifiers as "','quote_ident_with_dbl_"',
        'create table crash_me1 ("abc""d" integer)',
	'drop table crash_me1');		 

report("Column alias","column_alias","select a as ab from crash_me");
report("Table alias","table_alias","select b.a from crash_me as b");
report("Functions",'functions',"select 1+1 $end_query");
report("Group functions",'group_functions',"select count(*) from crash_me");
report("Group functions with distinct",'group_distinct_functions',
       "select count(distinct a) from crash_me");
report("Group functions with several distinct",'group_many_distinct_functions',
       "select count(distinct a), count(distinct b) from crash_me");
report("Group by",'group_by',"select a from crash_me group by a");
report("Group by position",'group_by_position',
       "select a from crash_me group by 1");
report("Group by alias",'group_by_alias',
       "select a as ab from crash_me group by ab");
report("Group on unused column",'group_on_unused',
       "select count(*) from crash_me group by a");

report("Order by",'order_by',"select a from crash_me order by a");
report("Order by position",'order_by_position',
       "select a from crash_me order by 1");
report("Order by function","order_by_function",
       "select a from crash_me order by a+1");
report("Order by on unused column",'order_on_unused',
       "select b from crash_me order by a");
# little bit deprecated
#check_and_report("Order by DESC is remembered",'order_by_remember_desc',
#		 ["create table crash_q (s int,s1 int)",
#		  "insert into crash_q values(1,1)",
#		  "insert into crash_q values(3,1)",
#		  "insert into crash_q values(2,1)"],
#		 "select s,s1 from crash_q order by s1 DESC,s",
#		 ["drop table crash_q $drop_attr"],[3,2,1],7,undef(),3);
report("Compute",'compute',
       "select a from crash_me order by a compute sum(a) by a");
report("INSERT with Value lists",'insert_multi_value',
       "create table crash_q (s char(10))",
       "insert into crash_q values ('a'),('b')",
       "drop table crash_q $drop_attr");
report("INSERT with set syntax",'insert_with_set',
       "create table crash_q (a integer)",
       "insert into crash_q SET a=1",
       "drop table crash_q $drop_attr");
report("INSERT with DEFAULT","insert_with_default",
       "create table crash_me_q (a int)",
       "insert into crash_me_q (a) values (DEFAULT)",
       "drop table crash_me_q $drop_attr");

report("INSERT with empty value list","insert_with_empty_value_list",
       "create table crash_me_q (a int)",
       "insert into crash_me_q (a) values ()",
       "drop table crash_me_q $drop_attr");

report("INSERT DEFAULT VALUES","insert_default_values",
       "create table crash_me_q (a int)",
       "insert into crash_me_q  DEFAULT VALUES",
       "drop table crash_me_q $drop_attr");
       
report("allows end ';'","end_colon", "select * from crash_me;");
try_and_report("LIMIT number of rows","select_limit",
	       ["with LIMIT",
		"select * from crash_me limit 1"],
	       ["with TOP",
		"select TOP 1 * from crash_me"]);
report("SELECT with LIMIT #,#","select_limit2", 
      "select * from crash_me limit 1,1");
report("SELECT with LIMIT # OFFSET #",
      "select_limit3", "select * from crash_me limit 1 offset 1");

# The following alter table commands MUST be kept together!
if ($dbh->do("create table crash_q (a integer, b integer,c1 CHAR(10))"))
{
  report("Alter table add column",'alter_add_col',
	 "alter table crash_q add d integer");
  report_one("Alter table add many columns",'alter_add_multi_col',
	     [["alter table crash_q add (f integer,g integer)","yes"],
	      ["alter table crash_q add f integer, add g integer","with add"],
	      ["alter table crash_q add f integer,g integer","without add"]] );
  report("Alter table change column",'alter_change_col',
	 "alter table crash_q change a e char(50)");

  # informix can only change data type with modify
  report_one("Alter table modify column",'alter_modify_col',
	     [["alter table crash_q modify c1 CHAR(20)","yes"],
	      ["alter table crash_q alter c1 CHAR(20)","with alter"]]);
  report("Alter table alter column default",'alter_alter_col',
	 "alter table crash_q alter b set default 10");
  report_one("Alter table drop column",'alter_drop_col',
	     [["alter table crash_q drop column b","yes"],
	      ["alter table crash_q drop column b restrict",
	      "with restrict/cascade"]]);
  report("Alter table rename table",'alter_rename_table',
	 "alter table crash_q rename to crash_q1");
}
# Make sure both tables will be dropped, even if rename fails.
$dbh->do("drop table crash_q1 $drop_attr");
$dbh->do("drop table crash_q $drop_attr");

report("rename table","rename_table",
       "create table crash_q (a integer, b integer,c1 CHAR(10))",
       "rename table crash_q to crash_q1",
       "drop table crash_q1 $drop_attr");
# Make sure both tables will be dropped, even if rename fails.
$dbh->do("drop table crash_q1 $drop_attr");
$dbh->do("drop table crash_q $drop_attr");

report("truncate","truncate_table",
       "create table crash_q (a integer, b integer,c1 CHAR(10))",
       "truncate table crash_q",
       "drop table crash_q $drop_attr");

if ($dbh->do("create table crash_q (a integer, b integer,c1 CHAR(10))") &&
 $dbh->do("create table crash_q1 (a integer, b integer,c1 CHAR(10) not null)"))
{
  report("Alter table add constraint",'alter_add_constraint',
	 "alter table crash_q add constraint c2 check(a > b)");
  report_one("Alter table drop constraint",'alter_drop_constraint',
	     [["alter table crash_q drop constraint c2","yes"],
	      ["alter table crash_q drop constraint c2 restrict",
	      "with restrict/cascade"]]);
  report("Alter table add unique",'alter_add_unique',
	 "alter table crash_q add constraint u1 unique(c1)");
  try_and_report("Alter table drop unique",'alter_drop_unique',
		 ["with constraint",
		  "alter table crash_q drop constraint u1"],
		 ["with constraint and restrict/cascade",
		  "alter table crash_q drop constraint u1 restrict"],
		 ["with drop key",
		  "alter table crash_q drop key u1"]);
  try_and_report("Alter table add primary key",'alter_add_primary_key',
		 ["with constraint",
		  "alter table crash_q1 add constraint p1 primary key(c1)"],
		 ["with add primary key",
		  "alter table crash_q1 add primary key(c1)"]);
  report("Alter table add foreign key",'alter_add_foreign_key',
	 "alter table crash_q add constraint f1 foreign key(c1) references crash_q1(c1)");
  try_and_report("Alter table drop foreign key",'alter_drop_foreign_key',
		 ["with drop constraint",
		  "alter table crash_q drop constraint f1"],
		 ["with drop constraint and restrict/cascade",
		  "alter table crash_q drop constraint f1 restrict"],
		 ["with drop foreign key",
		  "alter table crash_q drop foreign key f1"]);
  try_and_report("Alter table drop primary key",'alter_drop_primary_key',
		 ["drop constraint",
		  "alter table crash_q1 drop constraint p1 restrict"],
		 ["drop primary key",
		  "alter table crash_q1 drop primary key"]);
}
$dbh->do("drop table crash_q $drop_attr");
$dbh->do("drop table crash_q1 $drop_attr");

check_and_report("Case insensitive compare","case_insensitive_strings",
		 [],"select b from crash_me where b = 'A'",[],'a',1);
check_and_report("Ignore end space in compare","ignore_end_space",
		 [],"select b from crash_me where b = 'a '",[],'a',1);
check_and_report("Group on column with null values",'group_by_null',
		 ["create table crash_q (s char(10))",
		  "insert into crash_q values(null)",
		  "insert into crash_q values(null)"],
		 "select count(*),s from crash_q group by s",
		 ["drop table crash_q $drop_attr"],2,0);

$prompt="Having";
if (!defined($limits{'having'}))
{                               # Complicated because of postgreSQL
  if (!safe_query_result_l("having",
      "select a from crash_me group by a having a > 0",1,0))
  {
    if (!safe_query_result_l("having",
           "select a from crash_me group by a having a < 0",
	    1,0))
    { save_config_data("having","error",$prompt); }
    else
    { save_config_data("having","yes",$prompt); }
  }
  else
  { save_config_data("having","no",$prompt); }
}
print "$prompt: $limits{'having'}\n";

if ($limits{'having'} eq 'yes')
{
  report("Having with group function","having_with_group",
	 "select a from crash_me group by a having count(*) = 1");
}

if ($limits{'column_alias'} eq 'yes')
{
  report("Order by alias",'order_by_alias',
	 "select a as ab from crash_me order by ab");
  if ($limits{'having'} eq 'yes')
  {
    report("Having on alias","having_with_alias",
	   "select a as ab from crash_me group by a having ab > 0");
  }
}
report("binary numbers (0b1001)","binary_numbers","select 0b1001 $end_query");
report("hex numbers (0x41)","hex_numbers","select 0x41 $end_query");
report("binary strings (b'0110')","binary_strings","select b'0110' $end_query");
report("hex strings (x'1ace')","hex_strings","select x'1ace' $end_query");

report_result("Value of logical operation (1=1)","logical_value",
	      "select (1=1) $end_query");

report_result("Value of TRUE","value_of_true","select TRUE $end_query");
report_result("Value of FALSE","value_of_false","select FALSE $end_query");

$logical_value= $limits{'logical_value'};

$false=0;
$result="no";
if ($res=safe_query_l('has_true_false',"select (1=1)=true $end_query")) {
  $false="false";
  $result="yes";
}
save_config_data('has_true_false',$result,"TRUE and FALSE");

#
# Check how many connections the server can handle:
# We can't test unlimited connections, because this may take down the
# server...
#

$prompt="Simultaneous connections (installation default)";
print "$prompt: ";
if (defined($limits{'connections'}))
{
  print "$limits{'connections'}\n";
}
else
{
  @connect=($dbh);

  for ($i=1; $i < $max_connections ; $i++)
  {
    if (!($dbh=DBI->connect($server->{'data_source'},$opt_user,$opt_password,
			  { PrintError => 0})))
    {
      print "Last connect error: $DBI::errstr\n" if ($opt_debug);
      last;
    }
    $dbh->{LongReadLen}= $longreadlen; # Set retrieval buffer
    print "." if ($opt_debug);
    push(@connect,$dbh);
  }
  print "$i\n";
  save_config_data('connections',$i,$prompt);
  foreach $dbh (@connect)
  {
    print "#" if ($opt_debug);
    $dbh->disconnect || warn $dbh->errstr;           # close connection
  }

  $#connect=-1;                 # Free connections

  if ($i == 0)
  {
    print "Can't connect to server: $DBI::errstr.".
          "  Please start it and try again\n";
    exit 1;
  }
  $dbh=retry_connect();
}


#
# Check size of communication buffer, strings...
#

$prompt="query size";
print "$prompt: ";
if (!defined($limits{'query_size'}))
{
  $query="select ";
  $first=64;
  $end=$max_buffer_size;
  $select= $limits{'select_without_from'} eq 'yes' ? 1 : 'a';

  assert($query . "$select$end_query");

  $first=$limits{'restart'}{'low'} if ($limits{'restart'}{'low'});

  if ($limits{'restart'}{'tohigh'})
  {
    $end = $limits{'restart'}{'tohigh'} - 1;
    print "\nRestarting this with low limit: $first and high limit: $end\n";
    delete $limits{'restart'};
    $first=$first+int(($end-$first+4)/5);           # Prefere lower on errors
  }
  for ($i=$first ; $i < $end ; $i*=2)
  {
    last if (!safe_query($query . 
            (" " x ($i - length($query)-length($end_query) -1)) 
	      . "$select$end_query"));
    $first=$i;
    save_config_data("restart",$i,"") if ($opt_restart);
  }
  $end=$i;

  if ($i < $max_buffer_size)
  {
    while ($first != $end)
    {
      $i=int(($first+$end+1)/2);
      if (safe_query($query .
		     (" " x ($i - length($query)-length($end_query) -1)) .
		     "$select$end_query"))
      {
	$first=$i;
      }
      else
      {
	$end=$i-1;
      }
    }
  }
  save_config_data('query_size',$end,$prompt);
}
$query_size=$limits{'query_size'};

print "$limits{'query_size'}\n";

#
# Check for reserved words
#

check_reserved_words($dbh);

#
# Test database types
#

@sql_types=("character(1)","char(1)","char varying(1)", "character varying(1)",
	    "boolean",
	    "varchar(1)",
	    "integer","int","smallint",
	    "numeric(9,2)","decimal(6,2)","dec(6,2)",
	    "bit", "bit(2)","bit varying(2)","float","float(8)","real",
	    "double precision", "date","time","timestamp",
	    "interval year", "interval year to month",
            "interval month",
            "interval day", "interval day to hour", "interval day to minute",
            "interval day to second",
            "interval hour", "interval hour to minute",
	    "interval hour to second",
            "interval minute", "interval minute to second",
            "interval second",
	    "national character varying(20)",
	    "national character(20)","nchar(1)",
	    "national char varying(20)","nchar varying(20)",
	    "national character varying(20)",
	    "timestamp with time zone");
@odbc_types=("binary(1)","varbinary(1)","tinyint","bigint",
	     "datetime");
@extra_types=("blob","byte","long varbinary","image","text","text(10)",
	      "mediumtext",
	      "long varchar(1)", "varchar2(257)",
	      "mediumint","middleint","int unsigned",
	      "int1","int2","int3","int4","int8","uint",
	      "money","smallmoney","float4","float8","smallfloat",
	      "float(6,2)","double",
	      "enum('red')","set('red')", "int(5) zerofill", "serial",
	      "char(10) binary","int not null auto_increment,unique(q)",
	      "abstime","year","datetime","smalldatetime","timespan","reltime",
	      # Sybase types
	      "int not null identity,unique(q)",
	      # postgres types
	      "box","bool","circle","polygon","point","line","lseg","path",
	      "interval", "inet", "cidr", "macaddr",

	      # oracle types
	      "varchar2(16)","nvarchar2(16)","number(9,2)","number(9)",
	      "number", "long","raw(16)","long raw","rowid","mlslabel","clob",
	      "nclob","bfile"
	      );

@types=(["sql",\@sql_types],
	["odbc",\@odbc_types],
	["extra",\@extra_types]);

foreach $types (@types)
{
  print "\nSupported $types->[0] types\n";
  $tmp=@$types->[1];
  foreach $use_type (@$tmp)
  {
    $type=$use_type;
    $type =~ s/\(.*\)/(1 arg)/;
    if (index($use_type,",")>= 0)
    {
      $type =~ s/\(1 arg\)/(2 arg)/;
    }
    if (($tmp2=index($type,",unique")) >= 0)
    {
      $type=substr($type,0,$tmp2);
    }
    $tmp2=$type;
    $tmp2 =~ s/ /_/g;
    $tmp2 =~ s/_not_null//g;
    report("Type $type","type_$types->[0]_$tmp2",
	   "create table crash_q (q $use_type)",
	   "drop table crash_q $drop_attr");
  }
}

#
# Test some type limits
#


check_and_report("Remembers end space in char()","remember_end_space",
		 ["create table crash_q (a char(10))",
		  "insert into crash_q values('hello ')"],
		 "select a from crash_q where a = 'hello '",
		 ["drop table crash_q $drop_attr"],
		 'hello ',6);

check_and_report("Remembers end space in varchar()",
		 "remember_end_space_varchar",
		 ["create table crash_q (a varchar(10))",
		  "insert into crash_q values('hello ')"],
		 "select a from crash_q where a = 'hello '",
		 ["drop table crash_q $drop_attr"],
		 'hello ',6);

if (($limits{'type_extra_float(2_arg)'} eq "yes" ||
    $limits{'type_sql_decimal(2_arg)'} eq "yes") &&
    (!defined($limits{'storage_of_float'})))
{
  my $type=$limits{'type_extra_float(2_arg)'} eq "yes" ? "float(4,1)" :
    "decimal(4,1)";
  my $result="undefined";
  if (execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			 "insert into crash_q values(1.14)"],
			"select q1 from crash_q",
			["drop table crash_q $drop_attr"],1.1,0) &&
      execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			 "insert into crash_q values(1.16)"],
			"select q1 from crash_q",
			["drop table crash_q $drop_attr"],1.1,0))
  {
    $result="truncate";
  }
  elsif (execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.14)"],
			   "select q1 from crash_q",
			   ["drop table crash_q $drop_attr"],1.1,0) &&
	 execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.16)"],
			   "select q1 from crash_q",
			   ["drop table crash_q $drop_attr"],1.2,0))
  {
    $result="round";
  }
  elsif (execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.14)"],
			   "select q1 from crash_q",
			   ["drop table crash_q $drop_attr"],1.14,0) &&
	 execute_and_check("storage_of_float",["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.16)"],
			   "select q1 from crash_q",
			   ["drop table crash_q $drop_attr"],1.16,0))
  {
    $result="exact";
  }
  $prompt="Storage of float values";
  print "$prompt: $result\n";
  save_config_data("storage_of_float", $result, $prompt);
}

try_and_report("Type for row id", "rowid",
	       ["rowid",
		"create table crash_q (a rowid)",
		"drop table crash_q $drop_attr"],
	       ["auto_increment",
		"create table crash_q (a int not null auto_increment".
		", primary key(a))","drop table crash_q $drop_attr"],
	       ["oid",
		"create table crash_q (a oid, primary key(a))",
		"drop table crash_q $drop_attr"],
	       ["serial",
		"create table crash_q (a serial, primary key(a))",
		"drop table crash_q $drop_attr"]);

try_and_report("Automatic row id", "automatic_rowid",
	       ["_rowid",
		"create table crash_q (a int not null, primary key(a))",
		"insert into crash_q values (1)",
		"select _rowid from crash_q",
		"drop table crash_q $drop_attr"]);

#
# Test functions
#

@sql_functions=
  (["+, -, * and /","+","5*3-4/2+1",14,0],
   ["ANSI SQL SUBSTRING","substring","substring('abcd' from 2 for 2)","bc",1],
   ["BIT_LENGTH","bit_length","bit_length('abc')",24,0],
   ["searched CASE","searched_case",
     "case when 1 > 2 then 'false' when 2 > 1 then 'true' end", "true",1],
   ["simple CASE","simple_case",
     "case 2 when 1 then 'false' when 2 then 'true' end", "true",1],
   ["CAST","cast","CAST(1 as CHAR)","1",1],
   ["CHARACTER_LENGTH","character_length","character_length('abcd')","4",0],
   ["CHAR_LENGTH","char_length","char_length(b)","10",0],
   ["CHAR_LENGTH(constant)","char_length(constant)",
     "char_length('abcd')","4",0],
   ["COALESCE","coalesce","coalesce($char_null,'bcd','qwe')","bcd",1],
   ["CURRENT_DATE","current_date","current_date",0,2],
   ["CURRENT_TIME","current_time","current_time",0,2],
   ["CURRENT_TIMESTAMP","current_timestamp","current_timestamp",0,2],
   ["EXTRACT","extract_sql",
     "extract(minute from timestamp '2000-02-23 18:43:12.987')",43,0],
   ["LOCALTIME","localtime","localtime",0,2],
   ["LOCALTIMESTAMP","localtimestamp","localtimestamp",0,2],
   ["LOWER","lower","LOWER('ABC')","abc",1],
   ["NULLIF with strings","nullif_string",
       "NULLIF(NULLIF('first','second'),'first')",undef(),4],
   ["NULLIF with numbers","nullif_num","NULLIF(NULLIF(1,2),1)",undef(),4],
   ["OCTET_LENGTH","octet_length","octet_length('abc')",3,0],
   ["POSITION","position","position('ll' in 'hello')",3,0],
   ["TRIM","trim","trim(trailing from trim(LEADING FROM ' abc '))","abc",3],
   ["UPPER","upper","UPPER('abc')","ABC",1],
   ["concatenation with ||","concat_as_||","'abc' || 'def'","abcdef",1],
   );

@odbc_functions=
  (["ASCII", "ascii", "ASCII('A')","65",0],
   ["CHAR", "char", "CHAR(65)"  ,"A",1],
   ["CONCAT(2 arg)","concat", "concat('a','b')","ab",1],
   ["DIFFERENCE()","difference","difference('abc','abe')",3,0],
   ["INSERT","insert","insert('abcd',2,2,'ef')","aefd",1],
   ["LEFT","left","left('abcd',2)","ab",1],
   ["LTRIM","ltrim","ltrim('   abcd')","abcd",1],
   ["REAL LENGTH","length","length('abcd ')","5",0],
   ["ODBC LENGTH","length_without_space","length('abcd ')","4",0],
   ["LOCATE(2 arg)","locate_2","locate('bcd','abcd')","2",0],
   ["LOCATE(3 arg)","locate_3","locate('bcd','abcd',3)","0",0],
   ["LCASE","lcase","lcase('ABC')","abc",1],
   ["REPEAT","repeat","repeat('ab',3)","ababab",1],
   ["REPLACE","replace","replace('abbaab','ab','ba')","bababa",1],
   ["RIGHT","right","right('abcd',2)","cd",1],
   ["RTRIM","rtrim","rtrim(' abcd  ')"," abcd",1],
   ["SPACE","space","space(5)","     ",3],
   ["SOUNDEX","soundex","soundex('hello')",0,2],
   ["ODBC SUBSTRING","substring","substring('abcd',3,2)","cd",1],
   ["UCASE","ucase","ucase('abc')","ABC",1],

   ["ABS","abs","abs(-5)",5,0],
   ["ACOS","acos","acos(0)","1.570796",0],
   ["ASIN","asin","asin(1)","1.570796",0],
   ["ATAN","atan","atan(1)","0.785398",0],
   ["ATAN2","atan2","atan2(1,0)","1.570796",0],
   ["CEILING","ceiling","ceiling(-4.5)",-4,0],
   ["COS","cos","cos(0)","1.00000",0],
   ["COT","cot","cot(1)","0.64209262",0],
   ["DEGREES","degrees","degrees(6.283185)","360",0],
   ["EXP","exp","exp(1.0)","2.718282",0],
   ["FLOOR","floor","floor(2.5)","2",0],
   ["LOG","log","log(2)","0.693147",0],
   ["LOG10","log10","log10(10)","1",0],
   ["MOD","mod","mod(11,7)","4",0],
   ["PI","pi","pi()","3.141593",0],
   ["POWER","power","power(2,4)","16",0],
   ["RAND","rand","rand(1)",0,2],       # Any value is acceptable
   ["RADIANS","radians","radians(360)","6.283185",0],
   ["ROUND(2 arg)","round","round(5.63,2)","5.6",0],
   ["SIGN","sign","sign(-5)",-1,0],
   ["SIN","sin","sin(1)","0.841471",0],
   ["SQRT","sqrt","sqrt(4)",2,0],
   ["TAN","tan","tan(1)","1.557408",0],
   ["TRUNCATE","truncate","truncate(18.18,-1)",10,0],
   ["NOW","now","now()",0,2],           # Any value is acceptable
   ["CURDATE","curdate","curdate()",0,2],
   ["CURTIME","curtime","curtime()",0,2],
   ["TIMESTAMPADD","timestampadd",
    "timestampadd(SQL_TSI_SECOND,1,'1997-01-01 00:00:00')",
    "1997-01-01 00:00:01",1],
   ["TIMESTAMPDIFF","timestampdiff",
    "timestampdiff(SQL_TSI_SECOND,'1997-01-01 00:00:02',".
     " '1997-01-01 00:00:01')","1",0],
   ["USER()","user()","user()",0,2],
   ["DATABASE","database","database()",0,2],
   ["IFNULL","ifnull","ifnull(2,3)",2,0],
   ["ODBC syntax LEFT & RIGHT", "fn_left",
    "{ fn LEFT( { fn RIGHT('abcd',2) },1) }","c",1],
   );

@extra_functions=
  (
   ["& (bitwise and)",'&',"5 & 3",1,0],
   ["| (bitwise or)",'|',"1 | 2",3,0],
   ["<< and >> (bitwise shifts)",'binary_shifts',"(1 << 4) >> 2",4,0],
   ["<> in SELECT","<>","1<>1","0",0],
   ["=","=","(1=1)",1,$logical_value],
   ["~* (case insensitive compare)","~*","'hi' ~* 'HI'",1,$logical_value],
   ["AND and OR in SELECT","and_or","1=1 AND 2=2",$logical_value,0],
   ["AND as '&&'",'&&',"1=1 && 2=2",$logical_value,0],
   ["ASCII_CHAR", "ascii_char", "ASCII_CHAR(65)","A",1],
   ["ASCII_CODE", "ascii_code", "ASCII_CODE('A')","65",0],
   ["ATN2","atn2","atn2(1,0)","1.570796",0],
   ["BETWEEN in SELECT","between","5 between 4 and 6",$logical_value,0],
   ["BIT_COUNT","bit_count","bit_count(5)",2,0],
   ["CEIL","ceil","ceil(-4.5)",-4,0], # oracle
   ["CHARINDEX","charindex","charindex('a','crash')",3,0],
   ["CHR", "chr", "CHR(65)"  ,"A",1], # oracle
   ["CONCAT(list)","concat_list", "concat('a','b','c','d')","abcd",1],
   ["CONVERT","convert","convert(CHAR,5)","5",1],
   ["COSH","cosh","cosh(0)","1",0], # oracle hyperbolic cosine of n.
   ["ELT","elt","elt(2,'ONE','TWO','THREE')","TWO",1],
   ["ENCRYPT","encrypt","encrypt('hello')",0,2],
   ["FIELD","field","field('IBM','NCA','ICL','SUN','IBM','DIGITAL')",4,0],
   ["FORMAT","format","format(1234.5555,2)","1,234.56",1],
   ["GETDATE","getdate","getdate()",0,2],
   ["GREATEST","greatest","greatest('HARRY','HARRIOT','HAROLD')","HARRY",1],
   ["IF","if", "if(5,6,7)",6,0],
   ["IN on numbers in SELECT","in_num","2 in (3,2,5,9,5,1)",$logical_value,0],
   ["IN on strings in SELECT","in_str","'monty' in ('david','monty','allan')", $logical_value,0],
   ["INITCAP","initcap","initcap('the soap')","The Soap",1], 
       # oracle Returns char, with the first letter of each word in uppercase
   ["INSTR (Oracle syntax)", "instr_oracle", "INSTR('CORPORATE FLOOR','OR',3,2)"  ,"14",0], # oracle instring
   ["INSTRB", "instrb", "INSTRB('CORPORATE FLOOR','OR',5,2)"  ,"27",0], 
      # oracle instring in bytes
   ["INTERVAL","interval","interval(55,10,20,30,40,50,60,70,80,90,100)",5,0],
   ["LAST_INSERT_ID","last_insert_id","last_insert_id()",0,2],
   ["LEAST","least","least('HARRY','HARRIOT','HAROLD')","HAROLD",1], 
      # oracle
   ["LENGTHB","lengthb","lengthb('CANDIDE')","14",0], 
      # oracle length in bytes
   ["LIKE ESCAPE in SELECT","like_escape",
     "'%' like 'a%' escape 'a'",$logical_value,0],
   ["LIKE in SELECT","like","'a' like 'a%'",$logical_value,0],
   ["LN","ln","ln(95)","4.55387689",0], 
      # oracle natural logarithm of n
   ["LOCATE as INSTR","instr","instr('hello','ll')",3,0],
   ["LOG(m,n)","log(m_n)","log(10,100)","2",0], 
      # oracle logarithm, base m, of n
   ["LOGN","logn","logn(2)","0.693147",0], 
      # informix
   ["LPAD","lpad","lpad('hi',4,'??')",'??hi',3],
   ["MOD as %","%","10%7","3",0],
   ["NOT BETWEEN in SELECT","not_between","5 not between 4 and 6",0,0],
   ["NOT LIKE in SELECT","not_like","'a' not like 'a%'",0,0],
   ["NOT as '!' in SELECT","!","! 1",0,0],
   ["NOT in SELECT","not","not $false",$logical_value,0],
   ["ODBC CONVERT","odbc_convert","convert(5,SQL_CHAR)","5",1],
   ["OR as '||'",'||',"1=0 || 1=1",$logical_value,0],
   ["PASSWORD","password","password('hello')",0,2],
   ["PASTE", "paste", "paste('ABCDEFG',3,2,'1234')","AB1234EFG",1],
   ["PATINDEX","patindex","patindex('%a%','crash')",3,0],
   ["POW","pow","pow(3,2)",9,0],
   ["RANGE","range","range(a)","0.0",0], 
       # informix range(a) = max(a) - min(a)
   ["REGEXP in SELECT","regexp","'a' regexp '^(a|b)*\$'",$logical_value,0],
   ["REPLICATE","replicate","replicate('a',5)","aaaaa",1],
   ["REVERSE","reverse","reverse('abcd')","dcba",1],
   ["ROOT","root","root(4)",2,0], # informix
   ["ROUND(1 arg)","round1","round(5.63)","6",0],
   ["RPAD","rpad","rpad('hi',4,'??')",'hi??',3],
   ["SINH","sinh","sinh(1)","1.17520119",0], # oracle hyperbolic sine of n
   ["STR","str","str(123.45,5,1)",123.5,3],
   ["STRCMP","strcmp","strcmp('abc','adc')",-1,0],
   ["STUFF","stuff","stuff('abc',2,3,'xyz')",'axyz',3],
   ["SUBSTRB", "substrb", "SUBSTRB('ABCDEFG',5,4.2)"  ,"CD",1], 
      # oracle substring with bytes
   ["SUBSTRING as MID","mid","mid('hello',3,2)","ll",1],
   ["SUBSTRING_INDEX","substring_index",
     "substring_index('www.tcx.se','.',-2)", "tcx.se",1],
   ["SYSDATE","sysdate","sysdate()",0,2],
   ["TAIL","tail","tail('ABCDEFG',3)","EFG",0],
   ["TANH","tanh","tanh(1)","0.462117157",0], 
      # oracle hyperbolic tangent of n
   ["TRANSLATE","translate","translate('abc','bc','de')",'ade',3],
   ["TRIM; Many char extension",
     "trim_many_char","trim(':!' FROM ':abc!')","abc",3],
   ["TRIM; Substring extension",
     "trim_substring","trim('cb' FROM 'abccb')","abc",3],
   ["TRUNC","trunc","trunc(18.18,-1)",10,0], # oracle
   ["UID","uid","uid",0,2], # oracle uid from user
   ["UNIX_TIMESTAMP","unix_timestamp","unix_timestamp()",0,2],
   ["USERENV","userenv","userenv",0,2], # oracle user enviroment
   ["VERSION","version","version()",0,2],
   ["automatic num->string convert","auto_num2string","concat('a',2)","a2",1],
   ["automatic string->num convert","auto_string2num","'1'+2",3,0],
   ["concatenation with +","concat_as_+","'abc' + 'def'","abcdef",1],
   ["SUBSTR (2 arg)",'substr2arg',"substr('abcd',2)",'bcd',1],  #sapdb func
   ["SUBSTR (3 arg)",'substr3arg',"substr('abcd',2,2)",'bc',1],
   ["LFILL (3 arg)",'lfill3arg',"lfill('abcd','.',6)",'..abcd',1],
   ["RFILL (3 arg)",'rfill3arg',"rfill('abcd','.',6)",'abcd..',1],
   ["RPAD (4 arg)",'rpad4arg',"rpad('abcd',2,'+-',8)",'abcd+-+-',1],
   ["LPAD (4 arg)",'rpad4arg',"lpad('abcd',2,'+-',8)",'+-+-abcd',1],
   ["TRIM (1 arg)",'trim1arg',"trim(' abcd ')",'abcd',1],
   ["TRIM (2 arg)",'trim2arg',"trim('..abcd..','.')",'abcd',1],
   ["LTRIM (2 arg)",'ltrim2arg',"ltrim('..abcd..','.')",'abcd..',1],
   ["RTRIM (2 arg)",'rtrim2arg',"rtrim('..abcd..','.')",'..abcd',1],
   ["EXPAND",'expand2arg',"expand('abcd',6)",'abcd  ',0],
   ["REPLACE (2 arg) ",'replace2arg',"replace('AbCd','bC')",'Ad',1],
   ["MAPCHAR",'mapchar',"mapchar('Aâ')",'Aa',1],
   ["ALPHA",'alpha',"alpha('Aâ',2)",'AA',1],
   ["ASCII in string cast",'ascii_string',"ascii('a')",'a',1],
   ["EBCDIC in string cast",'ebcdic_string',"ebcdic('a')",'a',1],
   ["TRUNC (1 arg)",'trunc1arg',"trunc(222.6)",222,0],
   ["FIXED",'fixed',"fixed(222.6666,10,2)",'222.67',0],
   ["FLOAT",'float',"float(6666.66,4)",6667,0],
   ["LENGTH",'length',"length(1)",2,0],
   ["INDEX",'index',"index('abcdefg','cd',1,1)",3,0],
   ["MICROSECOND",'microsecond',
      "MICROSECOND('19630816200212111111')",'111111',0],
   ["TIMESTAMP",'timestamp',
      "timestamp('19630816','00200212')",'19630816200212000000',0],
   ["VALUE",'value',"value(NULL,'WALRUS')",'WALRUS',0],
   ["DECODE",'decode',"DECODE('S-103','T72',1,'S-103',2,'Leopard',3)",2,0],
   ["NUM",'num',"NUM('2123')",2123,0],
   ["CHR (any type to string)",'chr_str',"CHR(67)",'67',0],
   ["HEX",'hex',"HEX('A')",41,0],
   );


@sql_group_functions=
  (
   ["AVG","avg","avg(a)",1,0],
   ["COUNT (*)","count_*","count(*)",1,0],
   ["COUNT column name","count_column","count(a)",1,0],
   ["COUNT(DISTINCT expr)","count_distinct","count(distinct a)",1,0],
   ["MAX on numbers","max","max(a)",1,0],
   ["MAX on strings","max_str","max(b)","a",1],
   ["MIN on numbers","min","min(a)",1,0],
   ["MIN on strings","min_str","min(b)","a",1],
   ["SUM","sum","sum(a)",1,0],
   ["ANY","any","any(a)",$logical_value,0],
   ["EVERY","every","every(a)",$logical_value,0],
   ["SOME","some","some(a)",$logical_value,0],
   );

@extra_group_functions=
  (
   ["BIT_AND",'bit_and',"bit_and(a)",1,0],
   ["BIT_OR", 'bit_or', "bit_or(a)",1,0],
   ["COUNT(DISTINCT expr,expr,...)",
     "count_distinct_list","count(distinct a,b)",1,0],
   ["STD","std","std(a)",0,0],
   ["STDDEV","stddev","stddev(a)",0,0],
   ["VARIANCE","variance","variance(a)",0,0],
   );

@where_functions=
(
 ["= ALL","eq_all","b =all (select b from crash_me)",1,0],
 ["= ANY","eq_any","b =any (select b from crash_me)",1,0],
 ["= SOME","eq_some","b =some (select b from crash_me)",1,0],
 ["BETWEEN","between","5 between 4 and 6",1,0],
 ["EXISTS","exists","exists (select * from crash_me)",1,0],
 ["IN on numbers","in_num","2 in (3,2,5,9,5,1)",1,0],
 ["LIKE ESCAPE","like_escape","b like '%' escape 'a'",1,0],
 ["LIKE","like","b like 'a%'",1,0],
 ["MATCH UNIQUE","match_unique",
   "1 match unique (select a from crash_me)",1,0],
 ["MATCH","match","1 match (select a from crash_me)",1,0],
 ["MATCHES","matches","b matches 'a*'",1,0],
 ["NOT BETWEEN","not_between","7 not between 4 and 6",1,0],
 ["NOT EXISTS","not_exists",
   "not exists (select * from crash_me where a = 2)",1,0],
 ["NOT LIKE","not_like","b not like 'b%'",1,0],
 ["NOT UNIQUE","not_unique",
   "not unique (select * from crash_me where a = 2)",1,0],
 ["UNIQUE","unique","unique (select * from crash_me)",1,0],
 );

@types=(["sql",\@sql_functions,0],
	["odbc",\@odbc_functions,0],
	["extra",\@extra_functions,0],
	["where",\@where_functions,0]);

@group_types=(["sql",\@sql_group_functions,0],
	      ["extra",\@extra_group_functions,0]);


foreach $types (@types)
{
  print "\nSupported $types->[0] functions\n";
  $tmp=@$types->[1];
  foreach $type (@$tmp)
  {
    if (defined($limits{"func_$types->[0]_$type->[1]"}))
    {
      next;
    }
    if ($types->[0] eq "where")
    {
      check_and_report("Function $type->[0]","func_$types->[0]_$type->[1]",
		       [],"select a from crash_me where $type->[2]",[],
		       $type->[3],$type->[4]);
    }
    elsif ($limits{'functions'} eq 'yes')
    {
      if (($type->[2] =~ /char_length\(b\)/) && (!$end_query))
      {
	my $tmp= $type->[2];
	$tmp .= " from crash_me ";
	undef($limits{"func_$types->[0]_$type->[1]"});
	check_and_report("Function $type->[0]",
			 "func_$types->[0]_$type->[1]",
			 [],"select $tmp ",[],
			 $type->[3],$type->[4]);
      }
      else
      {
	undef($limits{"func_$types->[0]_$type->[1]"});
	$result = check_and_report("Function $type->[0]",
			    "func_$types->[0]_$type->[1]",
			    [],"select $type->[2] $end_query",[],
			    $type->[3],$type->[4]);
	if (!$result)
	{
	  # check without type specifyer
	  if ($type->[2] =~ /DATE /)
	  {
	    my $tmp= $type->[2];
	    $tmp =~ s/DATE //;
	    undef($limits{"func_$types->[0]_$type->[1]"});
	    $result = check_and_report("Function $type->[0]",
				  "func_$types->[0]_$type->[1]",
				  [],"select $tmp $end_query",[],
				  $type->[3],$type->[4]);
	  }
	  if (!$result)
	  {
	    if ($types->[0] eq "odbc" && ! ($type->[2] =~ /\{fn/))
	    {
	     my $tmp= $type->[2];
	     # Check by converting to ODBC format
	     undef($limits{"func_$types->[0]_$type->[1]"});
	     $tmp= "{fn $tmp }";
	     $tmp =~ s/('1997-\d\d-\d\d \d\d:\d\d:\d\d')/{ts $1}/g;
	     $tmp =~ s/(DATE '1997-\d\d-\d\d')/{d $1}/g;
	     $tmp =~ s/(TIME '12:13:14')/{t $1}/g;
	     $tmp =~ s/DATE //;
	     $tmp =~ s/TIME //;
	     check_and_report("Function $type->[0]",
			      "func_$types->[0]_$type->[1]",
			      [],"select $tmp $end_query",[],
			      $type->[3],$type->[4]);
	    }
	  }
        }
      }
    }
  }
}

if ($limits{'functions'} eq 'yes')
{
  foreach $types (@group_types)
  {
    print "\nSupported $types->[0] group functions\n";
    $tmp=@$types->[1];
    foreach $type (@$tmp)
    {
      check_and_report("Group function $type->[0]",
		       "group_func_$types->[0]_$type->[1]",
		       [],"select $type->[2],a from crash_me group by a",[],
		       $type->[3],$type->[4]);
    }
  }
  print "\n";
  report("mixing of integer and float in expression","float_int_expr",
	 "select 1+1.0 $end_query");
  if ($limits{'func_odbc_exp'} eq 'yes')
  {
    report("No need to cast from integer to float",
	   "dont_require_cast_to_float", "select exp(1) $end_query");
  }
  check_and_report("Is 1+NULL = NULL","null_num_expr",
		   [],"select 1+$numeric_null $end_query",[],undef(),4);
  $tmp=sql_concat("'a'",$char_null);
  if (defined($tmp))
  {
    check_and_report("Is $tmp = NULL", "null_concat_expr", [],
		     "select $tmp $end_query",[], undef(),4);
  }
  $prompt="Need to cast NULL for arithmetic";
  add_log("Need_cast_for_null",
    " Check if numeric_null ($numeric_null) is 'NULL'");
  save_config_data("Need_cast_for_null",
		   ($numeric_null eq "NULL") ? "no" : "yes",
		   $prompt);
}
else
{
  print "\n";
}


#  Test: NOROUND 
{
 my $result = 'undefined';
 my $error;
 print "NOROUND: ";
 save_incomplete('func_extra_noround','Function NOROUND');

# 1) check if noround() function is supported
 $error = safe_query_l('func_extra_noround',"select noround(22.6) $end_query");
 if ($error ne 1)         # syntax error -- noround is not supported 
 {
   $result = 'no'
 }
 else                   # Ok, now check if it really works
 {
   $error=safe_query_l('func_extra_noround', 
     ["create table crash_me_nr (a int)",
    "insert into crash_me_nr values(noround(10.2))",
    "drop table crash_me_nr $drop_attr"]);
   if ($error == 1)
   {
     $result= "syntax only";
   }
   else
   {
     $result= 'yes';
   }
 }
 print "$result\n";
 save_config_data('func_extra_noround',$result,"Function NOROUND");
}

check_parenthesis("func_sql_","CURRENT_USER");
check_parenthesis("func_sql_","SESSION_USER");
check_parenthesis("func_sql_","SYSTEM_USER");
check_parenthesis("func_sql_","USER");


if ($limits{'type_sql_date'} eq 'yes')
{  # 
   # Checking the format of date in result. 
   
    safe_query("drop table crash_me_d $drop_attr");
    assert("create table crash_me_d (a date)");
    # find the example of date
    my $dateexample;
    if ($limits{'func_extra_sysdate'} eq 'yes') {
     $dateexample=' sysdate() ';
    } 
    elsif ($limits{'func_sql_current_date'} eq 'yes') {
     $dateexample='CURRENT_DATE';
    } 
    elsif ($limits{'func_odbc_curdate'} eq 'yes') {
     $dateexample='curdate()';
    } 
    elsif ($limits{'func_extra_getdate'} eq 'yes') {
	$dateexample='getdate()';
    }
    elsif ($limits{'func_odbc_now'} eq 'yes') {
	$dateexample='now()';
    } else {
	#try to guess 
	$dateexample="DATE '1963-08-16'";
    } ;
    
    my $key = 'date_format_inresult';
    my $prompt = "Date format in result";
    if (! safe_query_l('date_format_inresult',
       "insert into crash_me_d values($dateexample) "))
    { 
	die "Cannot insert date ($dateexample):".$last_error; 
    };
    my $sth= $dbh->prepare("select a from crash_me_d");
    add_log('date_format_inresult',"< select a from crash_me_d");
    $sth->execute;
    $_= $sth->fetchrow_array;
    add_log('date_format_inresult',"> $_");
    safe_query_l($key,"delete from crash_me_d");   
    if (/\d{4}-\d{2}-\d{2}/){ save_config_data($key,"iso",$prompt);} 
    elsif (/\d{2}-\d{2}-\d{2}/){ save_config_data($key,"short iso",$prompt);}
    elsif (/\d{2}\.\d{2}\.\d{4}/){ save_config_data($key,"euro",$prompt);}
    elsif (/\d{2}\.\d{2}\.\d{2}/){ save_config_data($key,"short euro",$prompt);}
    elsif (/\d{2}\/\d{2}\/\d{4}/){ save_config_data($key,"usa",$prompt);}
    elsif (/\d{2}\/\d{2}\/\d{2}/){ save_config_data($key,"short usa",$prompt);}
    elsif (/\d*/){ save_config_data($key,"YYYYMMDD",$prompt);}
    else  { save_config_data($key,"unknown",$prompt);};
    $sth->finish;

    check_and_report("Supports YYYY-MM-DD (ISO) format","date_format_ISO",
		     [ "insert into crash_me_d(a)  values ('1963-08-16')"],
		     "select a from crash_me_d",
		     ["delete from crash_me_d"],
		     make_date_r(1963,8,16),1);

    check_and_report("Supports DATE 'YYYY-MM-DD' (ISO) format",
		     "date_format_ISO_with_date",
		     [ "insert into crash_me_d(a) values (DATE '1963-08-16')"],
		     "select a from crash_me_d",
		     ["delete from crash_me_d"],
		     make_date_r(1963,8,16),1);

    check_and_report("Supports DD.MM.YYYY (EUR) format","date_format_EUR",
		     [ "insert into crash_me_d(a) values ('16.08.1963')"],
		     "select a from crash_me_d",
		     ["delete from crash_me_d"],
		     make_date_r(1963,8,16),1);
    check_and_report("Supports DATE 'DD.MM.YYYY' (EUR) format",
		     "date_format_EUR_with_date",
		     [ "insert into crash_me_d(a) values (DATE '16.08.1963')"],
		     "select a from crash_me_d",
		     ["delete from crash_me_d"],
		     make_date_r(1963,8,16),1);

    check_and_report("Supports YYYYMMDD format",
	 "date_format_YYYYMMDD",
	 [ "insert into crash_me_d(a) values ('19630816')"],
	 "select a from crash_me_d",
	 ["delete from crash_me_d"],
	 make_date_r(1963,8,16),1);
    check_and_report("Supports DATE 'YYYYMMDD' format",
	 "date_format_YYYYMMDD_with_date",
	 [ "insert into crash_me_d(a) values (DATE '19630816')"],
	 "select a from crash_me_d",
	 ["delete from crash_me_d"],
	 make_date_r(1963,8,16),1);

    check_and_report("Supports MM/DD/YYYY format",
	 "date_format_USA",
	 [ "insert into crash_me_d(a) values ('08/16/1963')"],
	 "select a from crash_me_d",
	 ["delete from crash_me_d"],
	 make_date_r(1963,8,16),1);
    check_and_report("Supports DATE 'MM/DD/YYYY' format",
	 "date_format_USA_with_date",
	 [ "insert into crash_me_d(a) values (DATE '08/16/1963')"],
	 "select a from crash_me_d",
	 ["delete from crash_me_d"],
	 make_date_r(1963,8,16),1);


 

    check_and_report("Supports 0000-00-00 dates","date_zero",
	 ["create table crash_me2 (a date not null)",
	  "insert into crash_me2 values (".make_date(0,0,0).")"],
	 "select a from crash_me2",
	 ["drop table crash_me2 $drop_attr"],
	 make_date_r(0,0,0),1);

    check_and_report("Supports 0001-01-01 dates","date_one",
	 ["create table crash_me2 (a date not null)",
	  "insert into crash_me2 values (".make_date(1,1,1).")"],
	 "select a from crash_me2",
	 ["drop table crash_me2 $drop_attr"],
	 make_date_r(1,1,1),1);
    
    check_and_report("Supports 9999-12-31 dates","date_last",
	["create table crash_me2 (a date not null)",
        "insert into crash_me2 values (".make_date(9999,12,31).")"],
        "select a from crash_me2",
	["drop table crash_me2 $drop_attr"],
	make_date_r(9999,12,31),1);
    
    check_and_report("Supports 'infinity dates","date_infinity",
	 ["create table crash_me2 (a date not null)",
	 "insert into crash_me2 values ('infinity')"],
	 "select a from crash_me2",
	 ["drop table crash_me2 $drop_attr"],
	 "infinity",1);
    
    if (!defined($limits{'date_with_YY'}))
    {
	check_and_report("Supports YY-MM-DD dates","date_with_YY",
	   ["create table crash_me2 (a date not null)",
	   "insert into crash_me2 values ('98-03-03')"],
	   "select a from crash_me2",
	   ["drop table crash_me2 $drop_attr"],
	   make_date_r(1998,3,3),5);
	if ($limits{'date_with_YY'} eq "yes")
	{
	    undef($limits{'date_with_YY'});
	    check_and_report("Supports YY-MM-DD 2000 compilant dates",
	       "date_with_YY",
	       ["create table crash_me2 (a date not null)",
	       "insert into crash_me2 values ('10-03-03')"],
	       "select a from crash_me2",
	       ["drop table crash_me2 $drop_attr"],
	       make_date_r(2010,3,3),5);
	}
    }
    
# Test: WEEK()
    {
	my $result="no";
	my $error;
	print "WEEK:";
	save_incomplete('func_odbc_week','WEEK');
	$error = safe_query_result_l('func_odbc_week',
	     "select week(".make_date(1997,2,1).") $end_query",5,0);
	# actually this query must return 4 or 5 in the $last_result,
	# $error can be 1 (not supported at all) , -1 ( probably USA weeks)
	# and 0 - EURO weeks
	if ($error == -1) { 
	    if ($last_result == 4) {
		$result = 'USA';
	    } else {
		$result='error';
		add_log('func_odbc_week',
		  " must return 4 or 5, but $last_result");
	    }
	} elsif ($error == 0) {
	    $result = 'EURO';
	}
	print " $result\n";
	save_config_data('func_odbc_week',$result,"WEEK");
    }
    
    my $insert_query ='insert into crash_me_d values('.
        make_date(1997,2,1).')';
    safe_query($insert_query);
    
    foreach $fn ( (
		   ["DAYNAME","dayname","dayname(a)","",2],
		   ["MONTH","month","month(a)","",2],
		   ["MONTHNAME","monthname","monthname(a)","",2],
		   ["DAYOFMONTH","dayofmonth","dayofmonth(a)",1,0],
		   ["DAYOFWEEK","dayofweek","dayofweek(a)",7,0],
		   ["DAYOFYEAR","dayofyear","dayofyear(a)",32,0],
		   ["QUARTER","quarter","quarter(a)",1,0],
		   ["YEAR","year","year(a)",1997,0]))
    {
	$prompt='Function '.$fn->[0];
	$key='func_odbc_'.$fn->[1];
	add_log($key,"< ".$insert_query);
	check_and_report($prompt,$key,
			 [],"select ".$fn->[2]." from crash_me_d",[],
			 $fn->[3],$fn->[4]
			 );
	
    };
    safe_query(['delete from crash_me_d', 
		'insert into crash_me_d values('.make_date(1963,8,16).')']);
    foreach $fn ((
	  ["DATEADD","dateadd","dateadd(day,3,make_date(1997,11,30))",0,2],
	  ["MDY","mdy","mdy(7,1,1998)","make_date_r(1998,07,01)",0], # informix
	  ["DATEDIFF","datediff",
	     "datediff(month,'Oct 21 1997','Nov 30 1997')",0,2],
	  ["DATENAME","datename","datename(month,'Nov 30 1997')",0,2],
	  ["DATEPART","datepart","datepart(month,'July 20 1997')",0,2],
	  ["DATE_FORMAT","date_format", 
	    "date_format('1997-01-02 03:04:05','M W D Y y m d h i s w')", 0,2],
	  ["FROM_DAYS","from_days",
	    "from_days(729024)","make_date_r(1996,1,1)",1],
	  ["FROM_UNIXTIME","from_unixtime","from_unixtime(0)",0,2],
	  ["MONTHS_BETWEEN","months_between",
	   "months_between(make_date(1997,2,2),make_date(1997,1,1))",
	   "1.03225806",0], # oracle number of months between 2 dates
	  ["PERIOD_ADD","period_add","period_add(9602,-12)",199502,0],
	  ["PERIOD_DIFF","period_diff","period_diff(199505,199404)",13,0],
	  ["WEEKDAY","weekday","weekday(make_date(1997,11,29))",5,0],
	  ["ADDDATE",'adddate',
	   "ADDDATE(make_date(2002,12,01),3)",'make_date_r(2002,12,04)',0],
	  ["SUBDATE",'subdate',
	   "SUBDATE(make_date(2002,12,04),3)",'make_date_r(2002,12,01)',0],
	  ["DATEDIFF (2 arg)",'datediff2arg',
	   "DATEDIFF(make_date(2002,12,04),make_date(2002,12,01))",'3',0],
	  ["WEEKOFYEAR",'weekofyear',
	   "WEEKOFYEAR(make_date(1963,08,16))",'33',0],
# table crash_me_d must contain  record with 1963-08-16 (for CHAR)
	  ["CHAR (conversation date)",'char_date',
	   "CHAR(a,EUR)",'16.08.1963',0],
	  ["MAKEDATE",'makedate',"MAKEDATE(1963,228)"
	   ,'make_date_r(1963,08,16)',0],
	  ["TO_DAYS","to_days",
	   "to_days(make_date(1996,01,01))",729024,0],
	  ["ADD_MONTHS","add_months",
	   "add_months(make_date(1997,01,01),1)","make_date_r(1997,02,01)",0], 
	      # oracle the date plus n months
	  ["LAST_DAY","last_day",
	  "last_day(make_date(1997,04,01))","make_date_r(1997,04,30)",0], 
	      # oracle last day of month of date
	  ["DATE",'date',"date(make_date(1963,8,16))",
	     'make_date_r(1963,8,16)',0],
	  ["DAY",'day',"DAY(make_date(2002,12,01))",1,0]))
    {
	$prompt='Function '.$fn->[0];
	$key='func_extra_'.$fn->[1];
	my $qry="select ".$fn->[2]." from crash_me_d";
	while( $qry =~ /^(.*)make_date\((\d+),(\d+),(\d+)\)(.*)$/)
	{
	    my $dt= &make_date($2,$3,$4);
	    $qry=$1.$dt.$5;
	};
	my $result=$fn->[3];
	while( $result =~ /^(.*)make_date_r\((\d+),(\d+),(\d+)\)(.*)$/)
	{
	    my $dt= &make_date_r($2,$3,$4);
	    $result=$1.$dt.$5;
	};
	check_and_report($prompt,$key,
			 [],$qry,[],
			 $result,$fn->[4]
			 );
	
    }
    
    safe_query("drop table crash_me_d $drop_attr");    
    
}

if ($limits{'type_sql_time'} eq 'yes')
{  # 
   # Checking the format of date in result. 
   
    safe_query("drop table crash_me_t $drop_attr");
    assert("create table crash_me_t (a time)");
    # find the example of time
    my $timeexample;
    if ($limits{'func_sql_current_time'} eq 'yes') {
     $timeexample='CURRENT_TIME';
    } 
    elsif ($limits{'func_odbc_curtime'} eq 'yes') {
     $timeexample='curtime()';
    } 
    elsif ($limits{'func_sql_localtime'} eq 'yes') {
	$timeexample='localtime';
    }
    elsif ($limits{'func_odbc_now'} eq 'yes') {
	$timeexample='now()';
    } else {
	#try to guess 
	$timeexample="'02:55:12'";
    } ;
    
    my $key = 'time_format_inresult';
    my $prompt = "Time format in result";
    if (! safe_query_l('time_format_inresult',
       "insert into crash_me_t values($timeexample) "))
    { 
	die "Cannot insert time ($timeexample):".$last_error; 
    };
    my $sth= $dbh->prepare("select a from crash_me_t");
    add_log('time_format_inresult',"< select a from crash_me_t");
    $sth->execute;
    $_= $sth->fetchrow_array;
    add_log('time_format_inresult',"> $_");
    safe_query_l($key,"delete from crash_me_t");   
    if (/\d{2}:\d{2}:\d{2}/){ save_config_data($key,"iso",$prompt);} 
    elsif (/\d{2}\.\d{2}\.\d{2}/){ save_config_data($key,"euro",$prompt);}
    elsif (/\d{2}:\d{2}\s+(AM|PM)/i){ save_config_data($key,"usa",$prompt);}
    elsif (/\d{8}$/){ save_config_data($key,"HHHHMMSS",$prompt);}
    elsif (/\d{4}$/){ save_config_data($key,"HHMMSS",$prompt);}
    else  { save_config_data($key,"unknown",$prompt);};
    $sth->finish;

    check_and_report("Supports HH:MM:SS (ISO) time format","time_format_ISO",
		     [ "insert into crash_me_t(a)  values ('20:08:16')"],
		     "select a from crash_me_t",
		     ["delete from crash_me_t"],
		     make_time_r(20,8,16),1);

    check_and_report("Supports HH.MM.SS (EUR) time format","time_format_EUR",
		     [ "insert into crash_me_t(a) values ('20.08.16')"],
		     "select a from crash_me_t",
		     ["delete from crash_me_t"],
		     make_time_r(20,8,16),1);

    check_and_report("Supports HHHHmmSS time format",
	 "time_format_HHHHMMSS",
	 [ "insert into crash_me_t(a) values ('00200816')"],
	 "select a from crash_me_t",
	 ["delete from crash_me_t"],
	 make_time_r(20,8,16),1);

    check_and_report("Supports HHmmSS time format",
	 "time_format_HHHHMMSS",
	 [ "insert into crash_me_t(a) values ('200816')"],
	 "select a from crash_me_t",
	 ["delete from crash_me_t"],
	 make_time_r(20,8,16),1);
	 
    check_and_report("Supports HH:MM:SS (AM|PM) time format",
	 "time_format_USA",
	 [ "insert into crash_me_t(a) values ('08:08:16 PM')"],
	 "select a from crash_me_t",
	 ["delete from crash_me_t"],
	 make_time_r(20,8,16),1);	 
    
    my $insert_query ='insert into crash_me_t values('.
        make_time(20,8,16).')';
    safe_query($insert_query);
    
    foreach $fn ( (
            ["HOUR","hour","hour('".make_time(12,13,14)."')",12,0],
            ["ANSI HOUR","hour_time","hour(TIME '".make_time(12,13,14)."')",12,0],
            ["MINUTE","minute","minute('".make_time(12,13,14)."')",13,0],
            ["SECOND","second","second('".make_time(12,13,14)."')",14,0]

    ))
    {
	$prompt='Function '.$fn->[0];
	$key='func_odbc_'.$fn->[1];
	add_log($key,"< ".$insert_query);
	check_and_report($prompt,$key,
			 [],"select ".$fn->[2]." $end_query",[],
			 $fn->[3],$fn->[4]
			 );
	
    };
#    safe_query(['delete from crash_me_t', 
#		'insert into crash_me_t values('.make_time(20,8,16).')']);
    foreach $fn ((
         ["TIME_TO_SEC","time_to_sec","time_to_sec('".
	          make_time(1,23,21)."')","5001",0],
         ["SEC_TO_TIME","sec_to_time","sec_to_time(5001)",
	      make_time_r(01,23,21),1],
         ["ADDTIME",'addtime',"ADDTIME('".make_time(20,2,12).
	    "','".make_time(0,0,3)."')",make_time_r(20,2,15),0],
         ["SUBTIME",'subtime',"SUBTIME('".make_time(20,2,15)
	          ."','".make_time(0,0,3)."')",make_time_r(20,2,12),0],
         ["TIMEDIFF",'timediff',"TIMEDIFF('".make_time(20,2,15)."','".
	 make_time(20,2,12)."')",make_time_r(0,0,3),0],
         ["MAKETIME",'maketime',"MAKETIME(20,02,12)",make_time_r(20,2,12),0],
         ["TIME",'time',"time('".make_time(20,2,12)."')",make_time_r(20,2,12),0]
    ))
    {
	$prompt='Function '.$fn->[0];
	$key='func_extra_'.$fn->[1];
	my $qry="select ".$fn->[2]." $end_query";
	my $result=$fn->[3];
	check_and_report($prompt,$key,
			 [],$qry,[],
			 $result,$fn->[4]
			 );
	
    }
    
    safe_query("drop table crash_me_t $drop_attr");    
    
}


# NOT id BETWEEN a and b
if ($limits{'func_where_not_between'} eq 'yes')
{
   my $result = 'error';
   my $err;
   my $key='not_id_between';
   my $prompt='NOT ID BETWEEN interprets as ID NOT BETWEEN';
   print "$prompt:";
   save_incomplete($key,$prompt);
   safe_query_l($key,["create table crash_me_b (i int)",
         "insert into crash_me_b values(2)",
         "insert into crash_me_b values(5)"]);
   $err =safe_query_result_l($key,
    "select i from crash_me_b where not i between 1 and 3",
     5,0);
   if ($err eq 1) {
      if (not defined($last_result)) {
        $result='no';
      };
   };
   if ( $err eq 0) {
      $result = 'yes';
   };
   safe_query_l($key,["drop table crash_me_b"]);
   save_config_data($key,$result,$prompt);
   print "$result\n";
};




report("LIKE on numbers","like_with_number",
       "create table crash_q (a int,b int)",
       "insert into crash_q values(10,10)",
       "select * from crash_q where a like '10'",
       "drop table crash_q $drop_attr");

report("column LIKE column","like_with_column",
       "create table crash_q (a char(10),b char(10))",
       "insert into crash_q values('abc','abc')",
       "select * from crash_q where a like b",
       "drop table crash_q $drop_attr");

report("update of column= -column","NEG",
       "create table crash_q (a integer)",
       "insert into crash_q values(10)",
       "update crash_q set a=-a",
       "drop table crash_q $drop_attr");

if ($limits{'func_odbc_left'} eq 'yes' ||
    $limits{'func_odbc_substring'} eq 'yes')
{
  my $type= ($limits{'func_odbc_left'} eq 'yes' ?
	     "left(a,4)" : "substring(a for 4)");

    check_and_report("String functions on date columns","date_as_string",
		     ["create table crash_me2 (a date not null)",
		      "insert into crash_me2 values ('1998-03-03')"],
		     "select $type from crash_me2",
		     ["drop table crash_me2 $drop_attr"],
		     "1998",1);
}


$tmp=sql_concat("b","b");
if (defined($tmp))
{
  check_and_report("char are space filled","char_is_space_filled",
		   [],"select $tmp from crash_me where b = 'a         '",[],
		   'a         a         ',6);
}

if (!defined($limits{'multi_table_update'}))
{
  if (check_and_report("Update with many tables","multi_table_update",
	   ["create table crash_q (a integer,b char(10))",
	    "insert into crash_q values(1,'c')",
	    "update crash_q left join crash_me on crash_q.a=crash_me.a set crash_q.b=crash_me.b"],
            "select b from crash_q",
	   ["drop table crash_q $drop_attr"],
	   "a",1,undef(),2))
  {
    check_and_report("Update with many tables","multi_table_update",
	     ["create table crash_q (a integer,b char(10))",
	      "insert into crash_q values(1,'c')",
	      "update crash_q,crash_me set crash_q.b=crash_me.b ".
	      "where crash_q.a=crash_me.a"],
	     "select b from crash_q",
	     ["drop table crash_q $drop_attr"],
		     "a",1,
		    1);
  }
}

report("DELETE FROM table1,table2...","multi_table_delete",
       "create table crash_q (a integer,b char(10))",
       "insert into crash_q values(1,'c')",
       "delete crash_q.* from crash_q,crash_me where crash_q.a=crash_me.a",
       "drop table crash_q $drop_attr");

check_and_report("Update with sub select","select_table_update",
		 ["create table crash_q (a integer,b char(10))",
		  "insert into crash_q values(1,'c')",
		  "update crash_q set b= ".
		  "(select b from crash_me where crash_q.a = crash_me.a)"],
		 "select b from crash_q",
		 ["drop table crash_q $drop_attr"],
		 "a",1);

check_and_report("Calculate 1--1","minus_neg",[],
		 "select a--1 from crash_me",[],0,2);

report("ANSI SQL simple joins","simple_joins",
       "select crash_me.a from crash_me, crash_me t0");

#
# Check max string size, and expression limits
#
$found=undef;
foreach $type (('mediumtext','text','text()','blob','long'))
{
  if ($limits{"type_extra_$type"} eq 'yes')
  {
    $found=$type;
    last;
  }
}
if (defined($found))
{
  $found =~ s/\(\)/\(%d\)/;
  find_limit("max text or blob size","max_text_size",
	     new query_many(["create table crash_q (q $found)",
			     "insert into crash_q values ('%s')"],
			    "select * from crash_q","%s",
			    ["drop table crash_q $drop_attr"],
			    min($max_string_size,$limits{'query_size'}-30)));

}

# It doesn't make lots of sense to check for string lengths much bigger than
# what can be stored...

find_limit(($prompt="constant string size in where"),"where_string_size",
	   new query_repeat([],"select a from crash_me where b >='",
			    "","","1","","'"));
if ($limits{'where_string_size'} == 10)
{
  save_config_data('where_string_size','nonstandard',$prompt);
}

if ($limits{'select_constants'} eq 'yes')
{
  find_limit("constant string size in SELECT","select_string_size",
	     new query_repeat([],"select '","","","a","","'$end_query"));
}

goto no_functions if ($limits{'functions'} ne "yes");

if ($limits{'func_odbc_repeat'} eq 'yes')
{
  find_limit("return string size from function","repeat_string_size",
	     new query_many([],
			    "select repeat('a',%d) $end_query","%s",
			    [],
			    $max_string_size,0));
}

$tmp=find_limit("simple expressions","max_expressions",
		new query_repeat([],"select 1","","","+1","",$end_query,
				 undef(),$max_expressions));

if ($tmp > 10)
{
  $tmp= "(1" . ( '+1' x ($tmp-10) ) . ")";
  find_limit("big expressions", "max_big_expressions",
	     new query_repeat([],"select 0","","","+$tmp","",$end_query,
			      undef(),$max_big_expressions));
}

find_limit("stacked expressions", "max_stack_expression",
	   new query_repeat([],"select 1","","","+(1",")",$end_query,
				undef(),$max_stacked_expressions));

no_functions:

if (!defined($limits{'max_conditions'}))
{
  find_limit("OR and AND in WHERE","max_conditions",
	     new query_repeat([],
			      "select a from crash_me where a=1 and b='a'","",
			      "", " or a=%d and b='%d'","","","",
			      [],($query_size-42)/29,undef,2));
  $limits{'max_conditions'}*=2;
}
# The 42 is the length of the constant part.
# The 29 is the length of the variable part, plus two seven-digit numbers.

find_limit("tables in join", "join_tables",
	   new query_repeat([],
			    "select crash_me.a",",t%d.a","from crash_me",
			    ",crash_me t%d","","",[],$max_join_tables,undef,
			    1));

# Different CREATE TABLE options

report("primary key in create table",'primary_key_in_create',
       "create table crash_q (q integer not null,primary key (q))",
       "drop table crash_q $drop_attr");

report("unique in create table",'unique_in_create',
       "create table crash_q (q integer not null,unique (q))",
       "drop table crash_q $drop_attr");

if ($limits{'unique_in_create'} eq 'yes')
{
  report("unique null in create",'unique_null_in_create',
	 "create table crash_q (q integer,unique (q))",
	 "insert into crash_q (q) values (NULL)",
	 "insert into crash_q (q) values (NULL)",
	 "insert into crash_q (q) values (1)",
	 "drop table crash_q $drop_attr");
}

report("default value for column",'create_default',
       "create table crash_q (q integer default 10 not null)",
       "drop table crash_q $drop_attr");

report("default value function for column",'create_default_func',
       "create table crash_q (q integer not null,q1 integer default (1+1))",
       "drop table crash_q $drop_attr");

report("temporary tables",'temporary_table',
       "create temporary table crash_q (q integer not null)",
       "drop table crash_q $drop_attr");

report_one("create table from select",'create_table_select',
	   [["create table crash_q SELECT * from crash_me","yes"],
	    ["create table crash_q AS SELECT * from crash_me","with AS"]]);
$dbh->do("drop table crash_q $drop_attr");

report("index in create table",'index_in_create',
       "create table crash_q (q integer not null,index (q))",
       "drop table crash_q $drop_attr");

# The following must be executed as we need the value of end_drop_keyword
# later
if (!(defined($limits{'create_index'}) && defined($limits{'drop_index'})))
{
  if ($res=safe_query_l('create_index',"create index crash_q on crash_me (a)"))
  {
    $res="yes";
    $drop_res="yes";
    $end_drop_keyword="";
    if (!safe_query_l('drop_index',"drop index crash_q"))
    {
      # Can't drop the standard way; Check if mSQL
      if (safe_query_l('drop_index',"drop index crash_q from crash_me"))
      {
        $drop_res="with 'FROM'";	# Drop is not ANSI SQL
        $end_drop_keyword="drop index %i from %t";
      }
      # else check if Access or MySQL
      elsif (safe_query_l('drop_index',"drop index crash_q on crash_me"))
      {
        $drop_res="with 'ON'";	# Drop is not ANSI SQL
        $end_drop_keyword="drop index %i on %t";
      }
      # else check if MS-SQL
      elsif (safe_query_l('drop_index',"drop index crash_me.crash_q"))
      {
        $drop_res="with 'table.index'"; # Drop is not ANSI SQL
        $end_drop_keyword="drop index %t.%i";
      }
    }
    else
    {
      # Old MySQL 3.21 supports only the create index syntax
      # This means that the second create doesn't give an error.
      $res=safe_query_l('create_index',["create index crash_q on crash_me (a)",
      		     "create index crash_q on crash_me (a)",
      		     "drop index crash_q"]);
      $res= $res ? 'ignored' : 'yes';
    }
  }
  else
  {
    $drop_res=$res='no';
  }
  save_config_data('create_index',$res,"create index");
  save_config_data('drop_index',$drop_res,"drop index");

  print "create index: $limits{'create_index'}\n";
  print "drop index: $limits{'drop_index'}\n";
}

# check if we can have 'NULL' as a key
check_and_report("null in index","null_in_index",
		 [create_table("crash_q",["a char(10)"],["(a)"]),
		  "insert into crash_q values (NULL)"],
		 "select * from crash_q",
		 ["drop table crash_q $drop_attr"],
		 undef(),4);

if ($limits{'unique_in_create'} eq 'yes')
{
  report("null in unique index",'null_in_unique',
          create_table("crash_q",["q integer"],["unique(q)"]),
	 "insert into crash_q (q) values(NULL)",
	 "insert into crash_q (q) values(NULL)",
	 "drop table crash_q $drop_attr");
  report("null combination in unique index",'nulls_in_unique',
          create_table("crash_q",["q integer,q1 integer"],["unique(q,q1)"]),
	 "insert into crash_q (q,q1) values(1,NULL)",
	 "insert into crash_q (q,q1) values(1,NULL)",
	 "drop table crash_q $drop_attr");
}

if ($limits{'null_in_unique'} eq 'yes')
{
  report("null in unique index",'multi_null_in_unique',
          create_table("crash_q",["q integer, x integer"],["unique(q)"]),
	 "insert into crash_q(x) values(1)",
	 "insert into crash_q(x) values(2)",
	 "drop table crash_q $drop_attr");
}

if ($limits{'create_index'} ne 'no')
{
  $end_drop=$end_drop_keyword;
  $end_drop =~ s/%i/crash_q/;
  $end_drop =~ s/%t/crash_me/;
  report("index on column part (extension)","index_parts",,
	 "create index crash_q on crash_me (b(5))",
	 $end_drop);
  $end_drop=$end_drop_keyword;
  $end_drop =~ s/%i/crash_me/;
  $end_drop =~ s/%t/crash_me/;
  report("different namespace for index",
	 "index_namespace",
	 "create index crash_me on crash_me (b)",
	 $end_drop);
}

if (!report("case independent table names","table_name_case",
	    "create table crash_q (q integer)",
	    "drop table CRASH_Q $drop_attr"))
{
  safe_query("drop table crash_q $drop_attr");
}

if (!report("case independent field names","field_name_case",
	    "create table crash_q (q integer)",
	    "insert into crash_q(Q) values (1)",
	    "drop table crash_q $drop_attr"))
{
  safe_query("drop table crash_q $drop_attr");
}

if (!report("drop table if exists","drop_if_exists",
	    "create table crash_q (q integer)",
	    "drop table if exists crash_q $drop_attr"))
{
  safe_query("drop table crash_q $drop_attr");
}

report("create table if not exists","create_if_not_exists",
       "create table crash_q (q integer)",
       "create table if not exists crash_q (q integer)");
safe_query("drop table crash_q $drop_attr");

#
# test of different join types
#

assert("create table crash_me2 (a integer not null,b char(10) not null,".
       " c1 integer)");
assert("insert into crash_me2 (a,b,c1) values (1,'b',1)");
assert("create table crash_me3 (a integer not null,b char(10) not null)");
assert("insert into crash_me3 (a,b) values (1,'b')");

report("inner join","inner_join",
       "select crash_me.a from crash_me inner join crash_me2 ON ".
       "crash_me.a=crash_me2.a");
report("left outer join","left_outer_join",
       "select crash_me.a from crash_me left join crash_me2 ON ".
       "crash_me.a=crash_me2.a");
report("natural left outer join","natural_left_outer_join",
       "select c1 from crash_me natural left join crash_me2");
report("left outer join using","left_outer_join_using",
       "select c1 from crash_me left join crash_me2 using (a)");
report("left outer join odbc style","odbc_left_outer_join",
       "select crash_me.a from { oj crash_me left outer join crash_me2 ON".
       " crash_me.a=crash_me2.a }");
report("right outer join","right_outer_join",
       "select crash_me.a from crash_me right join crash_me2 ON ".
       "crash_me.a=crash_me2.a");
report("full outer join","full_outer_join",
       "select crash_me.a from crash_me full join crash_me2 ON "."
       crash_me.a=crash_me2.a");
report("cross join (same as from a,b)","cross_join",
       "select crash_me.a from crash_me cross join crash_me3");
report("natural join","natural_join",
       "select * from crash_me natural join crash_me3");
report("union","union",
       "select * from crash_me union select a,b from crash_me3");
report("union all","union_all",
       "select * from crash_me union all select a,b from crash_me3");
report("intersect","intersect",
       "select * from crash_me intersect select * from crash_me3");
report("intersect all","intersect_all",
       "select * from crash_me intersect all select * from crash_me3");
report("except","except",
       "select * from crash_me except select * from crash_me3");
report("except all","except_all",
       "select * from crash_me except all select * from crash_me3");
report("except","except",
       "select * from crash_me except select * from crash_me3");
report("except all","except_all",
       "select * from crash_me except all select * from crash_me3");
report("minus","minus",
       "select * from crash_me minus select * from crash_me3"); # oracle ...

report("natural join (incompatible lists)","natural_join_incompat",
       "select c1 from crash_me natural join crash_me2");
report("union (incompatible lists)","union_incompat",
       "select * from crash_me union select a,b from crash_me2");
report("union all (incompatible lists)","union_all_incompat",
       "select * from crash_me union all select a,b from crash_me2");
report("intersect (incompatible lists)","intersect_incompat",
       "select * from crash_me intersect select * from crash_me2");
report("intersect all (incompatible lists)","intersect_all_incompat",
       "select * from crash_me intersect all select * from crash_me2");
report("except (incompatible lists)","except_incompat",
       "select * from crash_me except select * from crash_me2");
report("except all (incompatible lists)","except_all_incompat",
       "select * from crash_me except all select * from crash_me2");
report("except (incompatible lists)","except_incompat",
       "select * from crash_me except select * from crash_me2");
report("except all (incompatible lists)","except_all_incompat",
       "select * from crash_me except all select * from crash_me2");
report("minus (incompatible lists)","minus_incompat",
       "select * from crash_me minus select * from crash_me2"); # oracle ...

assert("drop table crash_me2 $drop_attr");
assert("drop table crash_me3 $drop_attr");

# somethings to be added here ....
# FOR UNION - INTERSECT - EXCEPT -> CORRESPONDING [ BY ]
# after subqueries:
# >ALL | ANY | SOME - EXISTS - UNIQUE

if (report("subqueries","subqueries",
	   "select a from crash_me where crash_me.a in ".
	   "(select max(a) from crash_me)"))
{
    $tmp=new query_repeat([],"select a from crash_me","","",
			  " where a in (select a from crash_me",")",
			  "",[],$max_join_tables);
    find_limit("recursive subqueries", "recursive_subqueries",$tmp);
}

report("insert INTO ... SELECT ...","insert_select",
       "create table crash_q (a int)",
       "insert into crash_q (a) SELECT crash_me.a from crash_me",
       "drop table crash_q $drop_attr");

if (!defined($limits{"transactions"}))
{
  my ($limit,$type);
  $limit="transactions";
  $limit_r="rollback_metadata";
  print "$limit: ";
  foreach $type (('', 'type=bdb', 'type=innodb', 'type=gemini'))
  {
    undef($limits{$limit});
    if (!report_trans($limit,
			   [create_table("crash_q",["a integer not null"],[],
					 $type),
			    "insert into crash_q values (1)"],
			   "select * from crash_q",
			   "drop table crash_q $drop_attr"
			  ))
     {
       report_rollback($limit_r,
              [create_table("crash_q",["a integer not null"],[],
				 $type)],
			    "insert into crash_q values (1)",
			   "drop table crash_q $drop_attr" );
     last;
     };
  }
  print "$limits{$limit}\n";
  print "$limit_r: $limits{$limit_r}\n";
}

report("atomic updates","atomic_updates",
       create_table("crash_q",["a integer not null"],["primary key (a)"]),
       "insert into crash_q values (2)",
       "insert into crash_q values (3)",
       "insert into crash_q values (1)",
       "update crash_q set a=a+1",
       "drop table crash_q $drop_attr");

if ($limits{'atomic_updates'} eq 'yes')
{
  report_fail("atomic_updates_with_rollback","atomic_updates_with_rollback",
	      create_table("crash_q",["a integer not null"],
			   ["primary key (a)"]),
	      "insert into crash_q values (2)",
	      "insert into crash_q values (3)",
	      "insert into crash_q values (1)",
	      "update crash_q set a=a+1 where a < 3",
	      "drop table crash_q $drop_attr");
}

# To add with the views:
# DROP VIEW - CREAT VIEW *** [ WITH [ CASCADE | LOCAL ] CHECK OPTION ]
report("views","views",
       "create view crash_q as select a from crash_me",
       "drop view crash_q $drop_attr");

#  Test: foreign key
{
 my $result = 'undefined';
 my $error;
 print "foreign keys: ";
 save_incomplete('foreign_key','foreign keys');

# 1) check if foreign keys are supported
 safe_query_l('foreign_key',
	      create_table("crash_me_qf",
			   ["a integer not null"],
			   ["primary key (a)"]));
 $error= safe_query_l('foreign_key',
		      create_table("crash_me_qf2",
				   ["a integer not null",
				    "foreign key (a) references crash_me_qf (a)"],
				   []));

 if ($error == 1)         # OK  -- syntax is supported 
 {
   $result = 'error';
   # now check if foreign key really works
   safe_query_l('foreign_key', "insert into crash_me_qf values (1)");
   if (safe_query_l('foreign_key', "insert into crash_me_qf2 values (2)") eq 1)
   {
     $result = 'syntax only';
   }
   else
   {
     $result = 'yes';
   }
 }
 else
 {
   $result = "no";
 }
 safe_query_l('foreign_key', "drop table crash_me_qf2 $drop_attr");
 safe_query_l('foreign_key', "drop table crash_me_qf $drop_attr");
 print "$result\n";
 save_config_data('foreign_key',$result,"foreign keys");
}

if ($limits{'foreign_key'} eq 'yes')
{
  report("allows to update of foreign key values",'foreign_update',
   "create table crash_me1 (a int not null primary key)",
   "create table crash_me2 (a int not null," .
      " foreign key (a) references crash_me1 (a))",
   "insert into crash_me1 values (1)",
   "insert into crash_me2 values (1)",
   "update crash_me1 set a = 2",       ## <- must fail 
   "drop table crash_me2 $drop_attr", 
   "drop table crash_me1 $drop_attr" 
  );
}

report("Create SCHEMA","create_schema",
       "create schema crash_schema create table crash_q (a int) ".
       "create table crash_q2(b int)",
       "drop schema crash_schema cascade");

if ($limits{'foreign_key'} eq 'yes')
{
  if ($limits{'create_schema'} eq 'yes')
  {
    report("Circular foreign keys","foreign_key_circular",
           "create schema crash_schema create table crash_q ".
	   "(a int primary key, b int, foreign key (b) references ".
	   "crash_q2(a)) create table crash_q2(a int, b int, ".
	   "primary key(a), foreign key (b) references crash_q(a))",
           "drop schema crash_schema cascade");
  }
}

if ($limits{'func_sql_character_length'} eq 'yes')
{
  my $result = 'error';
  my ($resultset);
  my $key = 'length_of_varchar_field';
  my $prompt='CHARACTER_LENGTH(varchar_field)';
  print $prompt," = ";
  if (!defined($limits{$key})) {
    save_incomplete($key,$prompt);
    safe_query_l($key,[
		       "CREATE TABLE crash_me1 (S1 VARCHAR(100))",
		       "INSERT INTO crash_me1 VALUES ('X')"
		       ]);
    my $recset = get_recordset($key,
			       "SELECT CHARACTER_LENGTH(S1) FROM crash_me1");
    print_recordset($key,$recset);
    if (defined($recset)){
      if ( $recset->[0][0] eq 1 ) {
		$result = 'actual length';
	      } elsif( $recset->[0][0] eq 100 ) {
		$result = 'defined length';
	      };
    } else {
      add_log($key,$DBI::errstr);
    }
    safe_query_l($key, "drop table crash_me1 $drop_attr");
    save_config_data($key,$result,$prompt);
  } else {
    $result = $limits{$key};
  };
  print "$result\n";
}


check_constraint("Column constraints","constraint_check",
           "create table crash_q (a int check (a>0))",
           "insert into crash_q values(0)",
           "drop table crash_q $drop_attr");


check_constraint("Table constraints","constraint_check_table",
       "create table crash_q (a int ,b int, check (a>b))",
       "insert into crash_q values(0,0)",
       "drop table crash_q $drop_attr");

check_constraint("Named constraints","constraint_check_named",
       "create table crash_q (a int ,b int, constraint abc check (a>b))",
       "insert into crash_q values(0,0)",
       "drop table crash_q $drop_attr");


report("NULL constraint (SyBase style)","constraint_null",
       "create table crash_q (a int null)",
       "drop table crash_q $drop_attr");

report("Triggers (ANSI SQL)","psm_trigger",
       "create table crash_q (a int ,b int)",
       "create trigger crash_trigger after insert on crash_q referencing ".
       "new table as new_a when (localtime > time '18:00:00') ".
       "begin atomic end",
       "insert into crash_q values(1,2)",
       "drop trigger crash_trigger",
       "drop table crash_q $drop_attr");

report("PSM procedures (ANSI SQL)","psm_procedures",
       "create table crash_q (a int,b int)",
       "create procedure crash_proc(in a1 int, in b1 int) language ".
       "sql modifies sql data begin declare c1 int; set c1 = a1 + b1;".
       " insert into crash_q(a,b) values (a1,c1); end",
       "call crash_proc(1,10)",
       "drop procedure crash_proc",
       "drop table crash_q $drop_attr");

report("PSM modules (ANSI SQL)","psm_modules",
       "create table crash_q (a int,b int)",
       "create module crash_m declare procedure ".
         "crash_proc(in a1 int, in b1 int) language sql modifies sql ".
         "data begin declare c1 int; set c1 = a1 + b1; ".
         "insert into crash_q(a,b) values (a1,c1); end; ".
         "declare procedure crash_proc2(INOUT a int, in b int) ".
         "contains sql set a = b + 10; end module",
       "call crash_proc(1,10)",
       "drop module crash_m cascade",
       "drop table crash_q cascade $drop_attr");

report("PSM functions (ANSI SQL)","psm_functions",
       "create table crash_q (a int)",
       "create function crash_func(in a1 int, in b1 int) returns int".
         " language sql deterministic contains sql ".
	 " begin return a1 * b1; end",
       "insert into crash_q values(crash_func(2,4))",
       "select a,crash_func(a,2) from crash_q",
       "drop function crash_func cascade",
       "drop table crash_q $drop_attr");

report("Domains (ANSI SQL)","domains",
       "create domain crash_d as varchar(10) default 'Empty' ".
         "check (value <> 'abcd')",
       "create table crash_q(a crash_d, b int)",
       "insert into crash_q(a,b) values('xyz',10)",
       "insert into crash_q(b) values(10)",
       "drop table crash_q $drop_attr",
       "drop domain crash_d");


if (!defined($limits{'lock_tables'}))
{
  report("lock table","lock_tables",
	 "lock table crash_me READ",
	 "unlock tables");
  if ($limits{'lock_tables'} eq 'no')
  {
    delete $limits{'lock_tables'};
    report("lock table","lock_tables",
	   "lock table crash_me IN SHARE MODE");
  }
}

if (!report("many tables to drop table","multi_drop",
	   "create table crash_q (a int)",
	   "create table crash_q2 (a int)",
	   "drop table crash_q,crash_q2 $drop_attr"))
{
  $dbh->do("drop table crash_q $drop_attr");
  $dbh->do("drop table crash_q2 $drop_attr");
}

if (!report("drop table with cascade/restrict","drop_restrict",
	   "create table crash_q (a int)",
	   "drop table crash_q restrict"))
{
  $dbh->do("drop table crash_q $drop_attr");
}


report("-- as comment (ANSI)","comment_--",
       "select * from crash_me -- Testing of comments");
report("// as comment","comment_//",
       "select * from crash_me // Testing of comments");
report("# as comment","comment_#",
       "select * from crash_me # Testing of comments");
report("/* */ as comment","comment_/**/",
       "select * from crash_me /* Testing of comments */");

#
# Check things that fails one some servers
#

# Empress can't insert empty strings in a char() field
report("insert empty string","insert_empty_string",
       create_table("crash_q",["a char(10) not null,b char(10)"],[]),
       "insert into crash_q values ('','')",
       "drop table crash_q $drop_attr");

report("Having with alias","having_with_alias",
       create_table("crash_q",["a integer"],[]),
       "insert into crash_q values (10)",
       "select sum(a) as b from crash_q group by a having b > 0",
       "drop table crash_q $drop_attr");

#
# test name limits
#

find_limit("table name length","max_table_name",
	   new query_many(["create table crash_q%s (q integer)",
			   "insert into crash_q%s values(1)"],
			   "select * from crash_q%s",1,
			   ["drop table crash_q%s $drop_attr"],
			   $max_name_length,7,1));

find_limit("column name length","max_column_name",
	   new query_many(["create table crash_q (q%s integer)",
			  "insert into crash_q (q%s) values(1)"],
			  "select q%s from crash_q",1,
			  ["drop table crash_q $drop_attr"],
			   $max_name_length,1));

if ($limits{'column_alias'} eq 'yes')
{
  find_limit("select alias name length","max_select_alias_name",
	   new query_many(undef,
			  "select b as %s from crash_me",undef,
			  undef, $max_name_length));
}

find_limit("table alias name length","max_table_alias_name",
	   new query_many(undef,
			  "select %s.b from crash_me %s",
			  undef,
			  undef, $max_name_length));

$end_drop_keyword = "drop index %i" if (!$end_drop_keyword);
$end_drop=$end_drop_keyword;
$end_drop =~ s/%i/crash_q%s/;
$end_drop =~ s/%t/crash_me/;

if ($limits{'create_index'} ne 'no')
{
  find_limit("index name length","max_index_name",
	     new query_many(["create index crash_q%s on crash_me (a)"],
			    undef,undef,
			    [$end_drop],
			    $max_name_length,7));
}

find_limit("max char() size","max_char_size",
	   new query_many(["create table crash_q (q char(%d))",
			   "insert into crash_q values ('%s')"],
			  "select * from crash_q","%s",
			  ["drop table crash_q $drop_attr"],
			  min($max_string_size,$limits{'query_size'})));

if ($limits{'type_sql_varchar(1_arg)'} eq 'yes')
{
  find_limit("max varchar() size","max_varchar_size",
	     new query_many(["create table crash_q (q varchar(%d))",
			     "insert into crash_q values ('%s')"],
			    "select * from crash_q","%s",
			    ["drop table crash_q $drop_attr"],
			    min($max_string_size,$limits{'query_size'})));
}

$found=undef;
foreach $type (('mediumtext','text','text()','blob','long'))
{
  if ($limits{"type_extra_$type"} eq 'yes')
  {
    $found=$type;
    last;
  }
}
if (defined($found))
{
  $found =~ s/\(\)/\(%d\)/;
  find_limit("max text or blob size","max_text_size",
	     new query_many(["create table crash_q (q $found)",
			     "insert into crash_q values ('%s')"],
			    "select * from crash_q","%s",
			    ["drop table crash_q $drop_attr"],
			    min($max_string_size,$limits{'query_size'}-30)));

}

$tmp=new query_repeat([],"create table crash_q (a integer","","",
		      ",a%d integer","",")",["drop table crash_q $drop_attr"],
		      $max_columns);
$tmp->{'offset'}=1;
find_limit("Columns in table","max_columns",$tmp);

# Make a field definition to be used when testing keys

$key_definitions="q0 integer not null";
$key_fields="q0";
for ($i=1; $i < min($limits{'max_columns'},$max_keys) ; $i++)
{
  $key_definitions.=",q$i integer not null";
  $key_fields.=",q$i";
}
$key_values="1," x $i;
chop($key_values);

if ($limits{'unique_in_create'} eq 'yes')
{
  find_limit("unique indexes","max_unique_index",
	     new query_table("create table crash_q (q integer",
			     ",q%d integer not null,unique (q%d)",")",
			     ["insert into crash_q (q,%f) values (1,%v)"],
			     "select q from crash_q",1,
			     "drop table crash_q $drop_attr",
			     $max_keys,0));

  find_limit("index parts","max_index_parts",
	     new query_table("create table crash_q ".
	         "($key_definitions,unique (q0",
			     ",q%d","))",
 	     ["insert into crash_q ($key_fields) values ($key_values)"],
	     "select q0 from crash_q",1,
	     "drop table crash_q $drop_attr",
	     $max_keys,1));

  find_limit("max index part length","max_index_part_length",
	     new query_many(["create table crash_q (q char(%d) not null,".
	           "unique(q))",
		     "insert into crash_q (q) values ('%s')"],
		    "select q from crash_q","%s",
		    ["drop table crash_q $drop_attr"],
		    $limits{'max_char_size'},0));

  if ($limits{'type_sql_varchar(1_arg)'} eq 'yes')
  {
    find_limit("index varchar part length","max_index_varchar_part_length",
	     new query_many(["create table crash_q (q varchar(%d) not null,".
	                "unique(q))",
			 "insert into crash_q (q) values ('%s')"],
			"select q from crash_q","%s",
			["drop table crash_q $drop_attr"],
			$limits{'max_varchar_size'},0));
  }
}


if ($limits{'create_index'} ne 'no')
{
  if ($limits{'create_index'} eq 'ignored' ||
      $limits{'unique_in_create'} eq 'yes')
  {                                     # This should be true
    add_log('max_index',
     " max_unique_index=$limits{'max_unique_index'} ,".
     "so max_index must be same");
    save_config_data('max_index',$limits{'max_unique_index'},"max index");
    print "indexes: $limits{'max_index'}\n";
  }
  else
  {
    if (!defined($limits{'max_index'}))
    {
      safe_query_l('max_index',"create table crash_q ($key_definitions)");
      for ($i=1; $i <= min($limits{'max_columns'},$max_keys) ; $i++)
      {
	last if (!safe_query_l('max_index',
	     "create index crash_q$i on crash_q (q$i)"));
      }
      save_config_data('max_index',$i == $max_keys ? $max_keys : $i,
		       "max index");
      while ( --$i > 0)
      {
	$end_drop=$end_drop_keyword;
	$end_drop =~ s/%i/crash_q$i/;
	$end_drop =~ s/%t/crash_q/;
	assert($end_drop);
      }
      assert("drop table crash_q $drop_attr");
    }
    print "indexs: $limits{'max_index'}\n";
    if (!defined($limits{'max_unique_index'}))
    {
      safe_query_l('max_unique_index',
           "create table crash_q ($key_definitions)");
      for ($i=0; $i < min($limits{'max_columns'},$max_keys) ; $i++)
      {
	last if (!safe_query_l('max_unique_index',
	    "create unique index crash_q$i on crash_q (q$i)"));
      }
      save_config_data('max_unique_index',$i == $max_keys ? $max_keys : $i,
		       "max unique index");
      while ( --$i >= 0)
      {
	$end_drop=$end_drop_keyword;
	$end_drop =~ s/%i/crash_q$i/;
	$end_drop =~ s/%t/crash_q/;
	assert($end_drop);
      }
      assert("drop table crash_q $drop_attr");
    }
    print "unique indexes: $limits{'max_unique_index'}\n";
    if (!defined($limits{'max_index_parts'}))
    {
      safe_query_l('max_index_parts',
            "create table crash_q ($key_definitions)");
      $end_drop=$end_drop_keyword;
      $end_drop =~ s/%i/crash_q1%d/;
      $end_drop =~ s/%t/crash_q/;
      find_limit("index parts","max_index_parts",
		 new query_table("create index crash_q1%d on crash_q (q0",
				 ",q%d",")",
				 [],
				 undef,undef,
				 $end_drop,
				 $max_keys,1));
      assert("drop table crash_q $drop_attr");
    }
    else
    {
      print "index parts: $limits{'max_index_parts'}\n";
    }
    $end_drop=$end_drop_keyword;
    $end_drop =~ s/%i/crash_q2%d/;
    $end_drop =~ s/%t/crash_me/;

    find_limit("index part length","max_index_part_length",
	       new query_many(["create table crash_q (q char(%d))",
			       "create index crash_q2%d on crash_q (q)",
			       "insert into crash_q values('%s')"],
			      "select q from crash_q",
			      "%s",
			      [ $end_drop,
			       "drop table crash_q $drop_attr"],
			      min($limits{'max_char_size'},"+8192")));
  }
}

find_limit("index length","max_index_length",
	   new query_index_length("create table crash_q ",
				  "drop table crash_q $drop_attr",
				  $max_key_length));

find_limit("max table row length (without blobs)","max_row_length",
	   new query_row_length("crash_q ",
				"not null",
				"drop table crash_q $drop_attr",
				min($max_row_length,
				    $limits{'max_columns'}*
				    min($limits{'max_char_size'},255))));

find_limit("table row length with nulls (without blobs)",
	   "max_row_length_with_null",
	   new query_row_length("crash_q ",
				"",
				"drop table crash_q $drop_attr",
				$limits{'max_row_length'}*2));

find_limit("number of columns in order by","columns_in_order_by",
	   new query_many(["create table crash_q (%F)",
			   "insert into crash_q values(%v)",
			   "insert into crash_q values(%v)"],
			  "select * from crash_q order by %f",
			  undef(),
			  ["drop table crash_q $drop_attr"],
			  $max_order_by));

find_limit("number of columns in group by","columns_in_group_by",
	   new query_many(["create table crash_q (%F)",
			   "insert into crash_q values(%v)",
			   "insert into crash_q values(%v)"],
			  "select %f from crash_q group by %f",
			  undef(),
			  ["drop table crash_q $drop_attr"],
			  $max_order_by));



# Safe arithmetic test

$prompt="safe decimal arithmetic";
$key="safe_decimal_arithmetic";
if (!defined($limits{$key}))
{
   print "$prompt=";
   save_incomplete($key,$prompt);	
   if (!safe_query_l($key,$server->create("crash_me_a",
         ["a decimal(10,2)","b decimal(10,2)"]))) 
     {
       print DBI->errstr();
       die "Can't create table 'crash_me_a' $DBI::errstr\n";
     };
   
   if (!safe_query_l($key,
       ["insert into crash_me_a (a,b) values (11.4,18.9)"]))
     {
       die "Can't insert into table 'crash_me_a' a  record: $DBI::errstr\n";
     };
     
   $arithmetic_safe = 'no'; 
   $arithmetic_safe = 'yes' 
   if ( (safe_query_result_l($key,
            'select count(*) from crash_me_a where a+b=30.3',1,0) == 0) 
      and (safe_query_result_l($key,
            'select count(*) from crash_me_a where a+b-30.3 = 0',1,0) == 0)  
      and (safe_query_result_l($key,
            'select count(*) from crash_me_a where a+b-30.3 < 0',0,0) == 0)
      and (safe_query_result_l($key,
            'select count(*) from crash_me_a where a+b-30.3 > 0',0,0) == 0));
   save_config_data($key,$arithmetic_safe,$prompt);
   print "$arithmetic_safe\n";
   assert("drop table crash_me_a $drop_attr");
}
 else
{
  print "$prompt=$limits{$key} (cached)\n";
}

# Check where is null values in sorted recordset
if (!safe_query($server->create("crash_me_n",["i integer","r integer"]))) 
 {
   print DBI->errstr();
   die "Can't create table 'crash_me_n' $DBI::errstr\n";
 };
 
safe_query_l("position_of_null",["insert into crash_me_n (i) values(1)",
"insert into crash_me_n values(2,2)",
"insert into crash_me_n values(3,3)",
"insert into crash_me_n values(4,4)",
"insert into crash_me_n (i) values(5)"]);

$key = "position_of_null";
$prompt ="Where is null values in sorted recordset";
if (!defined($limits{$key}))
{
 save_incomplete($key,$prompt);	
 print "$prompt=";
 $sth=$dbh->prepare("select r from crash_me_n order by r ");
 $sth->execute;
 add_log($key,"< select r from crash_me_n order by r ");
 $limit= detect_null_position($key,$sth);
 $sth->finish;
 print "$limit\n";
 save_config_data($key,$limit,$prompt);
} else {
  print "$prompt=$limits{$key} (cache)\n";
}

$key = "position_of_null_desc";
$prompt ="Where is null values in sorted recordset (DESC)";
if (!defined($limits{$key}))
{
 save_incomplete($key,$prompt);	
 print "$prompt=";
 $sth=$dbh->prepare("select r from crash_me_n order by r desc");
 $sth->execute;
 add_log($key,"< select r from crash_me_n order by r  desc");
 $limit= detect_null_position($key,$sth);
 $sth->finish;
 print "$limit\n";
 save_config_data($key,$limit,$prompt);
} else {
  print "$prompt=$limits{$key} (cache)\n";
}


assert("drop table  crash_me_n $drop_attr");



$key = 'sorted_group_by';
$prompt = 'Group by always sorted';
if (!defined($limits{$key}))
{
 save_incomplete($key,$prompt);
 print "$prompt=";
 safe_query_l($key,[  
			 "create table crash_me_t1 (a int not null, b int not null)",
			 "insert into crash_me_t1 values (1,1)",
			 "insert into crash_me_t1 values (1,2)",
			 "insert into crash_me_t1 values (3,1)",
			 "insert into crash_me_t1 values (3,2)",
			 "insert into crash_me_t1 values (2,2)",
			 "insert into crash_me_t1 values (2,1)",
			 "create table crash_me_t2 (a int not null, b int not null)",
			 "create index crash_me_t2_ind on crash_me_t2 (a)",
			 "insert into crash_me_t2 values (1,3)",
			 "insert into crash_me_t2 values (3,1)",
			 "insert into crash_me_t2 values (2,2)",
			 "insert into crash_me_t2 values (1,1)"]);

 my $bigqry = "select crash_me_t1.a,crash_me_t2.b from ".
	     "crash_me_t1,crash_me_t2 where crash_me_t1.a=crash_me_t2.a ".
	     "group by crash_me_t1.a,crash_me_t2.b";

 my $limit='no';
 my $rs = get_recordset($key,$bigqry);
 print_recordset($key,$rs); 
 if ( defined ($rs)) { 
   if (compare_recordset($key,$rs,[[1,1],[1,3],[2,2],[3,1]]) eq 0)
   {
     $limit='yes'
   }
 } else {
  add_log($key,"error: ".$DBI::errstr);
 } 

 print "$limit\n";
 safe_query_l($key,["drop table crash_me_t1",
		       "drop table crash_me_t2"]);
 save_config_data($key,$limit,$prompt);	        
 
} else {
 print "$prompt=$limits{$key} (cashed)\n";
}


#
# End of test
#

$dbh->do("drop table crash_me $drop_attr");        # Remove temporary table

print "crash-me safe: $limits{'crash_me_safe'}\n";
print "reconnected $reconnect_count times\n";

$dbh->disconnect || warn $dbh->errstr;
save_all_config_data();
exit 0;

# End of test
#

$dbh->do("drop table crash_me $drop_attr");        # Remove temporary table

print "crash-me safe: $limits{'crash_me_safe'}\n";
print "reconnected $reconnect_count times\n";

$dbh->disconnect || warn $dbh->errstr;
save_all_config_data();
exit 0;

# Check where is nulls in the sorted result (for)
# it expects exactly 5 rows in the result

sub detect_null_position
{
  my $key = shift;
  my $sth = shift;
  my ($z,$r1,$r2,$r3,$r4,$r5);
 $r1 = $sth->fetchrow_array; add_log($key,"> $r1");
 $r2 = $sth->fetchrow_array; add_log($key,"> $r2");
 $r3 = $sth->fetchrow_array; add_log($key,"> $r3");
 $r4 = $sth->fetchrow_array; add_log($key,"> $r4");
 $r5 = $sth->fetchrow_array; add_log($key,"> $r5");
 return "first" if ( !defined($r1) && !defined($r2) && defined($r3));
 return "last" if ( !defined($r5) && !defined($r4) && defined($r3));
 return "random";
}

sub check_parenthesis {
 my $prefix=shift;
 my $fn=shift;
 my $result='no';
 my $param_name=$prefix.lc($fn);
 my $r;
 
 save_incomplete($param_name,$fn);
 $r = safe_query("select $fn $end_query"); 
 add_log($param_name,$safe_query_log);
 if ($r == 1)
  {
    $result="yes";
  } 
  else{
   $r = safe_query("select $fn() $end_query");
   add_log($param_name,$safe_query_log);
   if ( $r  == 1)   
    {    
       $result="with_parenthesis";
    }
  }

  save_config_data($param_name,$result,$fn);
}

sub check_constraint {
 my $prompt = shift;
 my $key = shift;
 my $create = shift;
 my $check = shift;
 my $drop = shift;
 save_incomplete($key,$prompt);
 print "$prompt=";
 my $res = 'no';
 my $t;
 $t=safe_query($create);
 add_log($key,$safe_query_log);
 if ( $t == 1)
 {
   $res='yes';
   $t= safe_query($check);
   add_log($key,$safe_query_log);
   if ($t == 1)
   {
     $res='syntax only';
   }
 }        
 safe_query($drop);
 add_log($key,$safe_query_log);
 
 save_config_data($key,$res,$prompt);
 print "$res\n";
}

sub make_time_r {
  my $hour=shift;
  my $minute=shift;
  my $second=shift;
  $_ = $limits{'time_format_inresult'};
  return sprintf "%02d:%02d:%02d", ($hour%24),$minute,$second if (/^iso$/);
  return sprintf "%02d.%02d.%02d", ($hour%24),$minute,$second if (/^euro/);
  return sprintf "%02d:%02d %s", 
        ($hour >= 13? ($hour-12) : $hour),$minute,($hour >=13 ? 'PM':'AM') 
	                if (/^usa/);
  return sprintf "%02d%02d%02d", ($hour%24),$minute,$second if (/^HHMMSS/);
  return sprintf "%04d%02d%02d", ($hour%24),$minute,$second if (/^HHHHMMSS/);
  return "UNKNOWN FORMAT";
}

sub make_time {
  my $hour=shift;
  my $minute=shift;
  my $second=shift;
  return sprintf "%02d:%02d:%02d", ($hour%24),$minute,$second 
      if ($limits{'time_format_ISO'} eq "yes");
  return sprintf "%02d.%02d.%02d", ($hour%24),$minute,$second 
      if ($limits{'time_format_EUR'} eq "yes");
  return sprintf "%02d:%02d %s", 
        ($hour >= 13? ($hour-12) : $hour),$minute,($hour >=13 ? 'PM':'AM') 
      if ($limits{'time_format_USA'} eq "yes");
  return sprintf "%02d%02d%02d", ($hour%24),$minute,$second 
      if ($limits{'time_format_HHMMSS'} eq "yes");
  return sprintf "%04d%02d%02d", ($hour%24),$minute,$second 
      if ($limits{'time_format_HHHHMMSS'} eq "yes");
  return "UNKNOWN FORMAT";
}

sub make_date_r {
  my $year=shift;
  my $month=shift;
  my $day=shift;
  $_ = $limits{'date_format_inresult'};
  return sprintf "%02d-%02d-%02d", ($year%100),$month,$day if (/^short iso$/);
  return sprintf "%04d-%02d-%02d", $year,$month,$day if (/^iso/);
  return sprintf "%02d.%02d.%02d", $day,$month,($year%100) if (/^short euro/);
  return sprintf "%02d.%02d.%04d", $day,$month,$year if (/^euro/);
  return sprintf "%02d/%02d/%02d", $month,$day,($year%100) if (/^short usa/);
  return sprintf "%02d/%02d/%04d", $month,$day,$year if (/^usa/);
  return sprintf "%04d%02d%02d", $year,$month,$day if (/^YYYYMMDD/);
  return "UNKNOWN FORMAT";
}


sub make_date {
  my $year=shift;
  my $month=shift;
  my $day=shift;
  return sprintf "'%04d-%02d-%02d'", $year,$month,$day 
      if ($limits{'date_format_ISO'} eq yes);
  return sprintf "DATE '%04d-%02d-%02d'", $year,$month,$day 
      if ($limits{'date_format_ISO_with_date'} eq yes);
  return sprintf "'%02d.%02d.%04d'", $day,$month,$year 
      if ($limits{'date_format_EUR'} eq 'yes');
  return sprintf "DATE '%02d.%02d.%04d'", $day,$month,$year 
      if ($limits{'date_format_EUR_with_date'} eq 'yes');
  return sprintf "'%02d/%02d/%04d'", $month,$day,$year 
      if ($limits{'date_format_USA'} eq 'yes');
  return sprintf "DATE '%02d/%02d/%04d'", $month,$day,$year 
      if ($limits{'date_format_USA_with_date'} eq 'yes');
  return sprintf "'%04d%02d%02d'", $year,$month,$day 
      if ($limits{'date_format_YYYYMMDD'} eq 'yes');
  return sprintf "DATE '%04d%02d%02d'", $year,$month,$day 
      if ($limits{'date_format_YYYYMMDD_with_date'} eq 'yes');
  return "UNKNOWN FORMAT";
}


sub print_recordset{
  my ($key,$recset) = @_;
  my $rec;
  foreach $rec (@$recset)
  {
    add_log($key, " > ".join(',', map(repr($_), @$rec)));
  }
}

#
# read result recordset from sql server. 
# returns arrayref to (arrayref to) values
# or undef (in case of sql errors)
#
sub get_recordset{
  my ($key,$query) = @_;
  add_log($key, "< $query");
  return $dbh->selectall_arrayref($query);
}

# function for comparing recordset (that was returned by get_recordset)
# and arrayref of (arrayref of) values.
#
# returns : zero if recordset equal that array, 1 if it doesn't equal
#
# parameters:
# $key - current operation (for logging)
# $recset - recordset
# $mustbe - array of values that we expect
#
# example: $a=get_recordset('some_parameter','select a,b from c');
# if (compare_recordset('some_parameter',$a,[[1,1],[1,2],[1,3]]) neq 0) 
# {
#   print "unexpected result\n";
# } ;
#
sub compare_recordset {
  my ($key,$recset,$mustbe) = @_;
  my $rec,$recno,$fld,$fldno,$fcount;
  add_log($key,"\n Check recordset:");
  $recno=0;
  foreach $rec (@$recset)
  {
    add_log($key," " . join(',', map(repr($_),@$rec)) . " expected: " .
	    join(',', map(repr($_), @{$mustbe->[$recno]} ) ));
    $fcount = @$rec;
    $fcount--;
    foreach $fldno (0 .. $fcount )
    {
      if ($mustbe->[$recno][$fldno] ne $rec->[$fldno])
      {
	add_log($key," Recordset doesn't correspond with template");
	return 1;
      };
    }
    $recno++;
  }
  add_log($key," Recordset corresponds with template");
  return 0;
}

#
# converts inner perl value to printable representation
# for example: undef maps to 'NULL',
# string -> 'string'
# int -> int
# 
sub repr {
  my $s = shift;
  return "'$s'"if ($s =~ /\D/);
  return 'NULL'if ( not defined($s));
  return $s;
}


sub version
{
  print "$0  Ver $version\n";
}


sub usage
{
  version();
    print <<EOF;

This program tries to find all limits and capabilities for a SQL
server.  As it will use the server in some 'unexpected' ways, one
shouldn\'t have anything important running on it at the same time this
program runs!  There is a slight chance that something unexpected may
happen....

As all used queries are legal according to some SQL standard. any
reasonable SQL server should be able to run this test without any
problems.

All questions is cached in $opt_dir/'server_name'[-suffix].cfg that
future runs will use limits found in previous runs. Remove this file
if you want to find the current limits for your version of the
database server.

This program uses some table names while testing things. If you have any
tables with the name of 'crash_me' or 'crash_qxxxx' where 'x' is a number,
they will be deleted by this test!

$0 takes the following options:

--help or --Information
  Shows this help

--batch-mode
  Don\'t ask any questions, quit on errors.

--config-file='filename'
  Read limit results from specific file

--comment='some comment'
  Add this comment to the crash-me limit file

--check-server
  Do a new connection to the server every time crash-me checks if the server
  is alive.  This can help in cases where the server starts returning wrong
  data because of an earlier select.

--database='database' (Default $opt_database)
  Create test tables in this database.

--dir='limits'
  Save crash-me output in this directory

--debug
  Lots of printing to help debugging if something goes wrong.

--fix-limit-file
  Reformat the crash-me limit file.  crash-me is not run!

--force
  Start test at once, without a warning screen and without questions.
  This is a option for the very brave.
  Use this in your cron scripts to test your database every night.

--log-all-queries
  Prints all queries that are executed. Mostly used for debugging crash-me.

--log-queries-to-file='filename'
  Log full queries to file.

--host='hostname' (Default $opt_host)
  Run tests on this host.

--password='password'
  Password for the current user.
   
--restart
  Save states during each limit tests. This will make it possible to continue
  by restarting with the same options if there is some bug in the DBI or
  DBD driver that caused $0 to die!

--server='server name'  (Default $opt_server)
  Run the test on the given server.
  Known servers names are: Access, Adabas, AdabasD, Empress, Oracle, 
  Informix, DB2, Mimer, mSQL, MS-SQL, MySQL, Pg, Solid or Sybase.
  For others $0 can\'t report the server version.

--suffix='suffix' (Default '')
  Add suffix to the output filename. For instance if you run crash-me like
  "crash-me --suffix="myisam",
  then output filename will look "mysql-myisam.cfg".

--user='user_name'
  User name to log into the SQL server.

--db-start-cmd='command to restart server'
  Automaticly restarts server with this command if the database server dies.

--sleep='time in seconds' (Default $opt_sleep)
  Wait this long before restarting server.

--verbose
--noverbose
  Log into the result file queries performed for determination parameter value

EOF
  exit(0);
}


sub server_info
{
  my ($ok,$tmp);
  $ok=0;
  print "\nNOTE: You should be familiar with '$0 --help' before continuing!\n\n";
  if (lc($opt_server) eq "mysql")
  {
    $ok=1;
    print <<EOF;
This test should not crash MySQL if it was distributed together with the
running MySQL version.
If this is the case you can probably continue without having to worry about
destroying something.
EOF
  }
  elsif (lc($opt_server) eq "msql")
  {
    print <<EOF;
This test will take down mSQL repeatedly while finding limits.
To make this test easier, start mSQL in another terminal with something like:

while (true); do /usr/local/mSQL/bin/msql2d ; done

You should be sure that no one is doing anything important with mSQL and that
you have privileges to restart it!
It may take awhile to determinate the number of joinable tables, so prepare to
wait!
EOF
  }
  elsif (lc($opt_server) eq "solid")
  {
    print <<EOF;
This test will take down Solid server repeatedly while finding limits.
You should be sure that no one is doing anything important with Solid
and that you have privileges to restart it!

If you are running Solid without logging and/or backup YOU WILL LOSE!
Solid does not write data from the cache often enough. So if you continue
you may lose tables and data that you entered hours ago!

Solid will also take a lot of memory running this test. You will nead
at least 234M free!

When doing the connect test Solid server or the perl api will hang when
freeing connections. Kill this program and restart it to continue with the
test. You don\'t have to use --restart for this case.
EOF
    if (!$opt_restart)
    {
      print "\nWhen DBI/Solid dies you should run this program repeatedly\n";
      print "with --restart until all tests have completed\n";
    }
  }
  elsif (lc($opt_server) eq "pg")
  {
    print <<EOF;
This test will crash postgreSQL when calculating the number of joinable tables!
You should be sure that no one is doing anything important with postgreSQL
and that you have privileges to restart it!
EOF
  }
  else
  {
    print <<EOF;
This test may crash $opt_server repeatedly while finding limits!
You should be sure that no one is doing anything important with $opt_server
and that you have privileges to restart it!
EOF
  }
  print <<EOF;

Some of the tests you are about to execute may require a lot of
memory.  Your tests WILL adversely affect system performance. It\'s
not uncommon that either this crash-me test program, or the actual
database back-end, will DIE with an out-of-memory error. So might
any other program on your system if it requests more memory at the
wrong time.

Note also that while crash-me tries to find limits for the database server
it will make a lot of queries that can\'t be categorized as \'normal\'.  It\'s
not unlikely that crash-me finds some limit bug in your server so if you
run this test you have to be prepared that your server may die during it!

We, the creators of this utility, are not responsible in any way if your
database server unexpectedly crashes while this program tries to find the
limitations of your server. By accepting the following question with \'yes\',
you agree to the above!

You have been warned!

EOF

  #
  # No default reply here so no one can blame us for starting the test
  # automaticly.
  #
  for (;;)
  {
    print "Start test (yes/no) ? ";
    $tmp=<STDIN>; chomp($tmp); $tmp=lc($tmp);
    last if ($tmp =~ /^yes$/i);
    exit 1 if ($tmp =~ /^n/i);
    print "\n";
  }
}

sub machine
{
  my @name = POSIX::uname();
  my $name= $name[0] . " " . $name[2] . " " . $name[4];
  return $name;
}


#
# Help functions that we need
#

sub safe_connect
{
  my ($object)=@_;
  my ($dbh,$tmp);

  for (;;)
  {
    if (($dbh=DBI->connect($server->{'data_source'},$opt_user,$opt_password,
			   { PrintError => 0, AutoCommit => 1})))
    {
      $dbh->{LongReadLen}= 16000000; # Set max retrieval buffer
      return $dbh;
    }
    print "Error: $DBI::errstr;  $server->{'data_source'} ".
        " - '$opt_user' - '$opt_password'\n";
    print "I got the above error when connecting to $opt_server\n";
    if (defined($object) && defined($object->{'limit'}))
    {
      print "This check was done with limit: $object->{'limit'}.".
          "\nNext check will be done with a smaller limit!\n";
      $object=undef();
    }
    save_config_data('crash_me_safe','no',"crash me safe");
    if ($opt_db_start_cmd)
    {
      print "Restarting the db server with:\n'$opt_db_start_cmd'\n";
      system("$opt_db_start_cmd");
      print "Waiting $opt_sleep seconds so the server can initialize\n";
      sleep $opt_sleep;
    }
    else
    {
      exit(1) if ($opt_batch_mode);
      print "Can you check/restart it so I can continue testing?\n";
      for (;;)
      {
	print "Continue test (yes/no) ? [yes] ";
	$tmp=<STDIN>; chomp($tmp); $tmp=lc($tmp);
	$tmp = "yes" if ($tmp eq "");
	last if (index("yes",$tmp) >= 0);
	exit 1 if (index("no",$tmp) >= 0);
	print "\n";
      }
    }
  }
}

#
# Test connecting a couple of times before giving an error
# This is needed to get the server time to free old connections
# after the connect test
#

sub retry_connect
{
  my ($dbh, $i);
  for ($i=0 ; $i < 10 ; $i++)
  {
    if (($dbh=DBI->connect($server->{'data_source'},$opt_user,$opt_password,
			 { PrintError => 0, AutoCommit => 1})))
    {
      $dbh->{LongReadLen}= 16000000; # Set max retrieval buffer
      return $dbh;
    }
    sleep(1);
  }
  return safe_connect();
}

#
# Check if the server is up and running. If not, ask the user to restart it
#

sub check_connect
{
  my ($object)=@_;
  my ($sth);
  print "Checking connection\n" if ($opt_log_all_queries);
  # The following line will not work properly with interbase
  if ($opt_check_server && defined($check_connect) && $dbh->{AutoCommit} != 0)
  {
    
    $dbh->disconnect;
    $dbh=safe_connect($object);
    return;
  }
  return if (defined($check_connect) && defined($dbh->do($check_connect)));
  $dbh->disconnect || warn $dbh->errstr;
  print "\nreconnecting\n" if ($opt_debug);
  $reconnect_count++;
  undef($dbh);
  $dbh=safe_connect($object);
}

#
# print query if debugging
#
sub repr_query {
  my $query=shift;
 if (length($query) > 130)
 {
   $query=substr($query,0,120) . "...(" . (length($query)-120) . ")";
 }
 return $query;
}  

sub print_query
{
  my ($query)=@_;
  $last_error=$DBI::errstr;
  if ($opt_debug)
  {
    if (length($query) > 130)
    {
      $query=substr($query,0,120) . "...(" . (length($query)-120) . ")";
    }
    printf "\nGot error from query: '%s'\n%s\n",$query,$DBI::errstr;
  }
}

#
# Do one or many queries. Return 1 if all was ok
# Note that all rows are executed 
# (to ensure that we execute drop table commands)
#

sub safe_query_l {
  my $key = shift;
  my $q = shift;
  my $r = safe_query($q);
  add_log($key,$safe_query_log);
  return $r;
}

sub safe_query
{
  my($queries)=@_;
  my($query,$ok,$retry_ok,$retry,@tmp,$sth);
  $safe_query_log="";
  $ok=1;
  if (ref($queries) ne "ARRAY")
  {
    push(@tmp,$queries);
    $queries= \@tmp;
  }
  foreach $query (@$queries)
  {
    printf "query1: %-80.80s ...(%d - %d)\n",$query,
          length($query),$retry_limit  if ($opt_log_all_queries);
    print LOG "$query;\n" if ($opt_log);
    $safe_query_log .= "< $query\n";
    if (length($query) > $query_size)
    {
      $ok=0;
      $safe_query_log .= "Query is too long\n";
      next;
    }

    $retry_ok=0;
    for ($retry=0; $retry < $retry_limit ; $retry++)
    {
      if (! ($sth=$dbh->prepare($query)))
      {
	print_query($query);
        $safe_query_log .= "> couldn't prepare:". $dbh->errstr. "\n";
	$retry=100 if (!$server->abort_if_fatal_error());
	# Force a reconnect because of Access drop table bug!
	if ($retry == $retry_limit-2)
	{
	  print "Forcing disconnect to retry query\n" if ($opt_debug);
	  $dbh->disconnect || warn $dbh->errstr;
	}
	check_connect();        # Check that server is still up
      }
      else
      {
        if (!$sth->execute())
        {
 	  print_query($query);
          $safe_query_log .= "> execute error:". $dbh->errstr. "\n";
	  $retry=100 if (!$server->abort_if_fatal_error());
	  # Force a reconnect because of Access drop table bug!
	  if ($retry == $retry_limit-2)
	  {
	    print "Forcing disconnect to retry query\n" if ($opt_debug);
	    $dbh->disconnect || warn $dbh->errstr;
	  }
	  check_connect();        # Check that server is still up
        }
        else
        {
	  $retry = $retry_limit;
	  $retry_ok = 1;
          $safe_query_log .= "> OK\n";
        }
        $sth->finish;
      }
    }
    $ok=0 if (!$retry_ok);
    if ($query =~ /create/i && $server->reconnect_on_errors())
    {
      print "Forcing disconnect to retry query\n" if ($opt_debug);
      $dbh->disconnect || warn $dbh->errstr;
      $dbh=safe_connect();
    }
  }
  return $ok;
}

sub check_reserved_words
{
  my ($dbh)= @_;

  my $answer, $prompt, $config, $keyword_type;

  my @keywords_ext  = ( "ansi-92/99", "ansi92", "ansi99", "extra");

  my %reserved_words = (
        'ABSOLUTE' =>  0,          'ACTION' =>  0,             'ADD' =>  0,
           'AFTER' =>  0,           'ALIAS' =>  0,             'ALL' =>  0,
        'ALLOCATE' =>  0,           'ALTER' =>  0,             'AND' =>  0,
             'ANY' =>  0,             'ARE' =>  0,              'AS' =>  0,
             'ASC' =>  0,       'ASSERTION' =>  0,              'AT' =>  0,
   'AUTHORIZATION' =>  0,          'BEFORE' =>  0,           'BEGIN' =>  0,
             'BIT' =>  0,         'BOOLEAN' =>  0,            'BOTH' =>  0,
         'BREADTH' =>  0,              'BY' =>  0,            'CALL' =>  0,
         'CASCADE' =>  0,        'CASCADED' =>  0,            'CASE' =>  0,
            'CAST' =>  0,         'CATALOG' =>  0,            'CHAR' =>  0,
       'CHARACTER' =>  0,           'CHECK' =>  0,           'CLOSE' =>  0,
         'COLLATE' =>  0,       'COLLATION' =>  0,          'COLUMN' =>  0,
          'COMMIT' =>  0,      'COMPLETION' =>  0,         'CONNECT' =>  0,
      'CONNECTION' =>  0,      'CONSTRAINT' =>  0,     'CONSTRAINTS' =>  0,
        'CONTINUE' =>  0,   'CORRESPONDING' =>  0,          'CREATE' =>  0,
           'CROSS' =>  0,         'CURRENT' =>  0,    'CURRENT_DATE' =>  0,
    'CURRENT_TIME' =>  0,'CURRENT_TIMESTAMP' =>  0,   'CURRENT_USER' =>  0,
          'CURSOR' =>  0,           'CYCLE' =>  0,            'DATA' =>  0,
            'DATE' =>  0,             'DAY' =>  0,      'DEALLOCATE' =>  0,
             'DEC' =>  0,         'DECIMAL' =>  0,         'DECLARE' =>  0,
         'DEFAULT' =>  0,      'DEFERRABLE' =>  0,        'DEFERRED' =>  0,
          'DELETE' =>  0,           'DEPTH' =>  0,            'DESC' =>  0,
        'DESCRIBE' =>  0,      'DESCRIPTOR' =>  0,     'DIAGNOSTICS' =>  0,
      'DICTIONARY' =>  0,      'DISCONNECT' =>  0,        'DISTINCT' =>  0,
          'DOMAIN' =>  0,          'DOUBLE' =>  0,            'DROP' =>  0,
            'EACH' =>  0,            'ELSE' =>  0,          'ELSEIF' =>  0,
             'END' =>  0,        'END-EXEC' =>  0,          'EQUALS' =>  0,
          'ESCAPE' =>  0,          'EXCEPT' =>  0,       'EXCEPTION' =>  0,
            'EXEC' =>  0,         'EXECUTE' =>  0,        'EXTERNAL' =>  0,
           'FALSE' =>  0,           'FETCH' =>  0,           'FIRST' =>  0,
           'FLOAT' =>  0,             'FOR' =>  0,         'FOREIGN' =>  0,
           'FOUND' =>  0,            'FROM' =>  0,            'FULL' =>  0,
         'GENERAL' =>  0,             'GET' =>  0,          'GLOBAL' =>  0,
              'GO' =>  0,            'GOTO' =>  0,           'GRANT' =>  0,
           'GROUP' =>  0,          'HAVING' =>  0,            'HOUR' =>  0,
        'IDENTITY' =>  0,              'IF' =>  0,          'IGNORE' =>  0,
       'IMMEDIATE' =>  0,              'IN' =>  0,       'INDICATOR' =>  0,
       'INITIALLY' =>  0,           'INNER' =>  0,           'INPUT' =>  0,
          'INSERT' =>  0,             'INT' =>  0,         'INTEGER' =>  0,
       'INTERSECT' =>  0,        'INTERVAL' =>  0,            'INTO' =>  0,
              'IS' =>  0,       'ISOLATION' =>  0,            'JOIN' =>  0,
             'KEY' =>  0,        'LANGUAGE' =>  0,            'LAST' =>  0,
         'LEADING' =>  0,           'LEAVE' =>  0,            'LEFT' =>  0,
            'LESS' =>  0,           'LEVEL' =>  0,            'LIKE' =>  0,
           'LIMIT' =>  0,           'LOCAL' =>  0,            'LOOP' =>  0,
           'MATCH' =>  0,          'MINUTE' =>  0,          'MODIFY' =>  0,
          'MODULE' =>  0,           'MONTH' =>  0,           'NAMES' =>  0,
        'NATIONAL' =>  0,         'NATURAL' =>  0,           'NCHAR' =>  0,
             'NEW' =>  0,            'NEXT' =>  0,              'NO' =>  0,
            'NONE' =>  0,             'NOT' =>  0,            'NULL' =>  0,
         'NUMERIC' =>  0,          'OBJECT' =>  0,              'OF' =>  0,
             'OFF' =>  0,             'OLD' =>  0,              'ON' =>  0,
            'ONLY' =>  0,            'OPEN' =>  0,       'OPERATION' =>  0,
          'OPTION' =>  0,              'OR' =>  0,           'ORDER' =>  0,
           'OUTER' =>  0,          'OUTPUT' =>  0,             'PAD' =>  0,
      'PARAMETERS' =>  0,         'PARTIAL' =>  0,       'PRECISION' =>  0,
        'PREORDER' =>  0,         'PREPARE' =>  0,        'PRESERVE' =>  0,
         'PRIMARY' =>  0,           'PRIOR' =>  0,      'PRIVILEGES' =>  0,
       'PROCEDURE' =>  0,          'PUBLIC' =>  0,            'READ' =>  0,
            'REAL' =>  0,       'RECURSIVE' =>  0,             'REF' =>  0,
      'REFERENCES' =>  0,     'REFERENCING' =>  0,        'RELATIVE' =>  0,
        'RESIGNAL' =>  0,        'RESTRICT' =>  0,          'RETURN' =>  0,
         'RETURNS' =>  0,          'REVOKE' =>  0,           'RIGHT' =>  0,
            'ROLE' =>  0,        'ROLLBACK' =>  0,         'ROUTINE' =>  0,
             'ROW' =>  0,            'ROWS' =>  0,       'SAVEPOINT' =>  0,
          'SCHEMA' =>  0,          'SCROLL' =>  0,          'SEARCH' =>  0,
          'SECOND' =>  0,         'SECTION' =>  0,          'SELECT' =>  0,
        'SEQUENCE' =>  0,         'SESSION' =>  0,    'SESSION_USER' =>  0,
             'SET' =>  0,          'SIGNAL' =>  0,            'SIZE' =>  0,
        'SMALLINT' =>  0,            'SOME' =>  0,           'SPACE' =>  0,
             'SQL' =>  0,    'SQLEXCEPTION' =>  0,        'SQLSTATE' =>  0,
      'SQLWARNING' =>  0,       'STRUCTURE' =>  0,     'SYSTEM_USER' =>  0,
           'TABLE' =>  0,       'TEMPORARY' =>  0,            'THEN' =>  0,
            'TIME' =>  0,       'TIMESTAMP' =>  0,   'TIMEZONE_HOUR' =>  0,
 'TIMEZONE_MINUTE' =>  0,              'TO' =>  0,        'TRAILING' =>  0,
     'TRANSACTION' =>  0,     'TRANSLATION' =>  0,         'TRIGGER' =>  0,
            'TRUE' =>  0,           'UNDER' =>  0,           'UNION' =>  0,
          'UNIQUE' =>  0,         'UNKNOWN' =>  0,          'UPDATE' =>  0,
           'USAGE' =>  0,            'USER' =>  0,           'USING' =>  0,
           'VALUE' =>  0,          'VALUES' =>  0,         'VARCHAR' =>  0,
        'VARIABLE' =>  0,         'VARYING' =>  0,            'VIEW' =>  0,
            'WHEN' =>  0,        'WHENEVER' =>  0,           'WHERE' =>  0,
           'WHILE' =>  0,            'WITH' =>  0,         'WITHOUT' =>  0,
            'WORK' =>  0,           'WRITE' =>  0,            'YEAR' =>  0,
            'ZONE' =>  0,

           'ASYNC' =>  1,             'AVG' =>  1,         'BETWEEN' =>  1,
      'BIT_LENGTH' =>  1,'CHARACTER_LENGTH' =>  1,     'CHAR_LENGTH' =>  1,
        'COALESCE' =>  1,         'CONVERT' =>  1,           'COUNT' =>  1,
          'EXISTS' =>  1,         'EXTRACT' =>  1,     'INSENSITIVE' =>  1,
           'LOWER' =>  1,             'MAX' =>  1,             'MIN' =>  1,
          'NULLIF' =>  1,    'OCTET_LENGTH' =>  1,             'OID' =>  1,
       'OPERATORS' =>  1,          'OTHERS' =>  1,        'OVERLAPS' =>  1,
         'PENDANT' =>  1,        'POSITION' =>  1,         'PRIVATE' =>  1,
       'PROTECTED' =>  1,         'REPLACE' =>  1,       'SENSITIVE' =>  1,
         'SIMILAR' =>  1,         'SQLCODE' =>  1,        'SQLERROR' =>  1,
       'SUBSTRING' =>  1,             'SUM' =>  1,            'TEST' =>  1,
           'THERE' =>  1,       'TRANSLATE' =>  1,            'TRIM' =>  1,
            'TYPE' =>  1,           'UPPER' =>  1,         'VIRTUAL' =>  1,
         'VISIBLE' =>  1,            'WAIT' =>  1,

           'ADMIN' =>  2,       'AGGREGATE' =>  2,           'ARRAY' =>  2,
          'BINARY' =>  2,            'BLOB' =>  2,           'CLASS' =>  2,
            'CLOB' =>  2,       'CONDITION' =>  2,     'CONSTRUCTOR' =>  2,
        'CONTAINS' =>  2,            'CUBE' =>  2,    'CURRENT_PATH' =>  2,
    'CURRENT_ROLE' =>  2,        'DATALINK' =>  2,           'DEREF' =>  2,
         'DESTROY' =>  2,      'DESTRUCTOR' =>  2,   'DETERMINISTIC' =>  2,
              'DO' =>  2,         'DYNAMIC' =>  2,           'EVERY' =>  2,
            'EXIT' =>  2,          'EXPAND' =>  2,       'EXPANDING' =>  2,
            'FREE' =>  2,        'FUNCTION' =>  2,        'GROUPING' =>  2,
         'HANDLER' =>  2,            'HAST' =>  2,            'HOST' =>  2,
      'INITIALIZE' =>  2,           'INOUT' =>  2,         'ITERATE' =>  2,
           'LARGE' =>  2,         'LATERAL' =>  2,       'LOCALTIME' =>  2,
  'LOCALTIMESTAMP' =>  2,         'LOCATOR' =>  2,           'MEETS' =>  2,
        'MODIFIES' =>  2,           'NCLOB' =>  2,       'NORMALIZE' =>  2,
      'ORDINALITY' =>  2,             'OUT' =>  2,       'PARAMETER' =>  2,
            'PATH' =>  2,          'PERIOD' =>  2,         'POSTFIX' =>  2,
        'PRECEDES' =>  2,          'PREFIX' =>  2,           'READS' =>  2,
            'REDO' =>  2,          'REPEAT' =>  2,          'RESULT' =>  2,
          'ROLLUP' =>  2,            'SETS' =>  2,        'SPECIFIC' =>  2,
    'SPECIFICTYPE' =>  2,           'START' =>  2,           'STATE' =>  2,
          'STATIC' =>  2,        'SUCCEEDS' =>  2,       'TERMINATE' =>  2,
            'THAN' =>  2,           'TREAT' =>  2,            'UNDO' =>  2,
           'UNTIL' =>  2,

          'ACCESS' =>  3,         'ANALYZE' =>  3,           'AUDIT' =>  3,
  'AUTO_INCREMENT' =>  3,          'BACKUP' =>  3,             'BDB' =>  3,
      'BERKELEYDB' =>  3,          'BIGINT' =>  3,           'BREAK' =>  3,
          'BROWSE' =>  3,           'BTREE' =>  3,            'BULK' =>  3,
          'CHANGE' =>  3,      'CHECKPOINT' =>  3,         'CLUSTER' =>  3,
       'CLUSTERED' =>  3,         'COLUMNS' =>  3,         'COMMENT' =>  3,
        'COMPRESS' =>  3,         'COMPUTE' =>  3,   'CONTAINSTABLE' =>  3,
        'DATABASE' =>  3,       'DATABASES' =>  3,        'DAY_HOUR' =>  3,
      'DAY_MINUTE' =>  3,      'DAY_SECOND' =>  3,            'DBCC' =>  3,
         'DELAYED' =>  3,            'DENY' =>  3,            'DISK' =>  3,
     'DISTINCTROW' =>  3,     'DISTRIBUTED' =>  3,           'DUMMY' =>  3,
            'DUMP' =>  3,        'ENCLOSED' =>  3,          'ERRLVL' =>  3,
          'ERRORS' =>  3,         'ESCAPED' =>  3,       'EXCLUSIVE' =>  3,
         'EXPLAIN' =>  3,          'FIELDS' =>  3,            'FILE' =>  3,
      'FILLFACTOR' =>  3,        'FREETEXT' =>  3,   'FREETEXTTABLE' =>  3,
        'FULLTEXT' =>  3,        'GEOMETRY' =>  3,            'HASH' =>  3,
   'HIGH_PRIORITY' =>  3,        'HOLDLOCK' =>  3,     'HOUR_MINUTE' =>  3,
     'HOUR_SECOND' =>  3,      'IDENTIFIED' =>  3,     'IDENTITYCOL' =>  3,
 'IDENTITY_INSERT' =>  3,       'INCREMENT' =>  3,           'INDEX' =>  3,
          'INFILE' =>  3,         'INITIAL' =>  3,          'INNODB' =>  3,
            'KEYS' =>  3,            'KILL' =>  3,          'LINENO' =>  3,
           'LINES' =>  3,            'LOAD' =>  3,            'LOCK' =>  3,
            'LONG' =>  3,        'LONGBLOB' =>  3,        'LONGTEXT' =>  3,
    'LOW_PRIORITY' =>  3, 'MASTER_SERVER_ID' =>  3,      'MAXEXTENTS' =>  3,
      'MEDIUMBLOB' =>  3,       'MEDIUMINT' =>  3,      'MEDIUMTEXT' =>  3,
       'MIDDLEINT' =>  3,           'MINUS' =>  3,   'MINUTE_SECOND' =>  3,
        'MLSLABEL' =>  3,            'MODE' =>  3,      'MRG_MYISAM' =>  3,
         'NOAUDIT' =>  3,         'NOCHECK' =>  3,      'NOCOMPRESS' =>  3,
    'NONCLUSTERED' =>  3,          'NOWAIT' =>  3,          'NUMBER' =>  3,
         'OFFLINE' =>  3,         'OFFSETS' =>  3,          'ONLINE' =>  3,
  'OPENDATASOURCE' =>  3,       'OPENQUERY' =>  3,      'OPENROWSET' =>  3,
         'OPENXML' =>  3,        'OPTIMIZE' =>  3,      'OPTIONALLY' =>  3,
         'OUTFILE' =>  3,            'OVER' =>  3,         'PCTFREE' =>  3,
         'PERCENT' =>  3,            'PLAN' =>  3,           'PRINT' =>  3,
            'PROC' =>  3,           'PURGE' =>  3,       'RAISERROR' =>  3,
             'RAW' =>  3,        'READTEXT' =>  3,     'RECONFIGURE' =>  3,
          'REGEXP' =>  3,          'RENAME' =>  3,     'REPLICATION' =>  3,
         'REQUIRE' =>  3,        'RESOURCE' =>  3,         'RESTORE' =>  3,
           'RLIKE' =>  3,        'ROWCOUNT' =>  3,      'ROWGUIDCOL' =>  3,
           'ROWID' =>  3,          'ROWNUM' =>  3,           'RTREE' =>  3,
            'RULE' =>  3,            'SAVE' =>  3,         'SETUSER' =>  3,
           'SHARE' =>  3,            'SHOW' =>  3,        'SHUTDOWN' =>  3,
          'SONAME' =>  3,         'SPATIAL' =>  3,  'SQL_BIG_RESULT' =>  3,
'SQL_CALC_FOUND_ROWS' =>  3,'SQL_SMALL_RESULT' =>  3,        'SSL' =>  3,
        'STARTING' =>  3,      'STATISTICS' =>  3,   'STRAIGHT_JOIN' =>  3,
         'STRIPED' =>  3,      'SUCCESSFUL' =>  3,         'SYNONYM' =>  3,
         'SYSDATE' =>  3,          'TABLES' =>  3,      'TERMINATED' =>  3,
        'TEXTSIZE' =>  3,        'TINYBLOB' =>  3,         'TINYINT' =>  3,
        'TINYTEXT' =>  3,             'TOP' =>  3,            'TRAN' =>  3,
        'TRUNCATE' =>  3,         'TSEQUAL' =>  3,           'TYPES' =>  3,
             'UID' =>  3,          'UNLOCK' =>  3,        'UNSIGNED' =>  3,
      'UPDATETEXT' =>  3,             'USE' =>  3,  'USER_RESOURCES' =>  3,
        'VALIDATE' =>  3,       'VARBINARY' =>  3,        'VARCHAR2' =>  3,
         'WAITFOR' =>  3,        'WARNINGS' =>  3,       'WRITETEXT' =>  3,
             'XOR' =>  3,      'YEAR_MONTH' =>  3,        'ZEROFILL' =>  3
);


  safe_query("drop table crash_me10 $drop_attr");

  foreach my $keyword (sort {$a cmp $b} keys %reserved_words)
  {
    $keyword_type= $reserved_words{$keyword};

    $prompt= "Keyword ".$keyword;
    $config= "reserved_word_".$keywords_ext[$keyword_type]."_".lc($keyword);

    report_fail($prompt,$config,
      "create table crash_me10 ($keyword int not null)",
      "drop table crash_me10 $drop_attr"
    );
  }
}

#
# Do a query on a query package object.
#

sub limit_query
{
  my($object,$limit)=@_;
  my ($query,$result,$retry,$sth);

  $query=$object->query($limit);
  $result=safe_query($query);
  if (!$result)
  {
    $object->cleanup();
    return 0;
  }
  if (defined($query=$object->check_query()))
  {
    for ($retry=0 ; $retry < $retry_limit ; $retry++)
    {
      printf "query2: %-80.80s\n",$query if ($opt_log_all_queries);
      print LOG "$query;\n" if ($opt_log);
      if (($sth= $dbh->prepare($query)))
      {
	if ($sth->execute)
	{
	  $result= $object->check($sth);
	  $sth->finish;
	  $object->cleanup();
	  return $result;
	}
	print_query($query);
	$sth->finish;
      }
      else
      {
	print_query($query);
      }
      $retry=100 if (!$server->abort_if_fatal_error()); # No need to continue
      if ($retry == $retry_limit-2)
      {
	print "Forcing discoennect to retry query\n" if ($opt_debug);
	$dbh->disconnect || warn $dbh->errstr;
      }
      check_connect($object);   # Check that server is still up
    }
    $result=0;                  # Query failed
  }
  $object->cleanup();
  return $result;               # Server couldn't handle the query
}


sub report
{
  my ($prompt,$limit,@queries)=@_;
  print "$prompt: ";
  if (!defined($limits{$limit}))
  {
    my $queries_result = safe_query(\@queries);
    add_log($limit, $safe_query_log);
    my $report_result;
    if ( $queries_result) {
      $report_result= "yes";
      add_log($limit,"As far as all queries returned OK, result is YES");
    } else {
      $report_result= "no";
      add_log($limit,"As far as some queries didnt return OK, result is NO");
    } 
    save_config_data($limit,$report_result,$prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
}

sub report_fail
{
  my ($prompt,$limit,@queries)=@_;
  print "$prompt: ";
  if (!defined($limits{$limit}))
  {
    my $queries_result = safe_query(\@queries);
    add_log($limit, $safe_query_log);
    my $report_result;
    if ( $queries_result) {
      $report_result= "no";
      add_log($limit,"As far as all queries returned OK, result is NO");
    } else {
      $report_result= "yes";
      add_log($limit,"As far as some queries didnt return OK, result is YES");
    } 
    save_config_data($limit,$report_result,$prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
}


# Return true if one of the queries is ok

sub report_one
{
  my ($prompt,$limit,$queries)=@_;
  my ($query,$res,$result);
  print "$prompt: ";
  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    $result="no";
    foreach $query (@$queries)
    {
      if (safe_query_l($limit,$query->[0]))
      {
	$result= $query->[1];
	last;
      }
    }
    save_config_data($limit,$result,$prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
}


# Execute query and save result as limit value.

sub report_result
{
  my ($prompt,$limit,$query)=@_;
  my($error);
  print "$prompt: ";
  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    $error=safe_query_result($query,"1",2);
    add_log($limit,$safe_query_result_log);
    save_config_data($limit,$error ? "not supported" :$last_result,$prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "not supported";
}

sub report_trans
{
  my ($limit,$queries,$check,$clear)=@_;
  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    eval {undef($dbh->{AutoCommit})};
    if (!$@)
    {
      if (safe_query(\@$queries))
      {
	  $dbh->rollback;
          $dbh->{AutoCommit} = 1;
	    if (safe_query_result($check,"","")) {
              add_log($limit,$safe_query_result_log);	    
	      save_config_data($limit,"yes",$limit);
	    }
	    safe_query($clear);
      } else {
        add_log($limit,$safe_query_log);
        save_config_data($limit,"error",$limit);
      }
      $dbh->{AutoCommit} = 1;
    }
    else
    {
      add_log($limit,"Couldnt undef autocommit ?? ");
      save_config_data($limit,"no",$limit);
    }
    safe_query($clear);
  }
  return $limits{$limit} ne "yes";
}

sub report_rollback
{
  my ($limit,$queries,$check,$clear)=@_;
  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    eval {undef($dbh->{AutoCommit})};
    if (!$@)
    {
      if (safe_query(\@$queries))
      {
          add_log($limit,$safe_query_log);

	  $dbh->rollback;
           $dbh->{AutoCommit} = 1;
           if (safe_query($check)) {
	      add_log($limit,$safe_query_log);
	      save_config_data($limit,"no",$limit);
	    }  else  {
	      add_log($limit,$safe_query_log);
	      save_config_data($limit,"yes",$limit);
	    };
	    safe_query($clear);
      } else {
        add_log($limit,$safe_query_log);
        save_config_data($limit,"error",$limit);
      }
    }
    else
    {
      add_log($limit,'Couldnt undef Autocommit??');
      save_config_data($limit,"error",$limit);
    }
    safe_query($clear);
  }
  $dbh->{AutoCommit} = 1;
  return $limits{$limit} ne "yes";
}


sub check_and_report
{
  my ($prompt,$limit,$pre,$query,$post,$answer,$string_type,$skip_prompt,
      $function)=@_;
  my ($tmp);
  $function=0 if (!defined($function));

  print "$prompt: " if (!defined($skip_prompt));
  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    $tmp=1-safe_query(\@$pre);
    add_log($limit,$safe_query_log);
    if (!$tmp) 
    {
        $tmp=safe_query_result($query,$answer,$string_type) ;
        add_log($limit,$safe_query_result_log);
    };	
    safe_query(\@$post);
    add_log($limit,$safe_query_log);
    delete $limits{$limit};
    if ($function == 3)		# Report error as 'no'.
    {
      $function=0;
      $tmp= -$tmp;
    }
    if ($function == 0 ||
	$tmp != 0 && $function == 1 ||
	$tmp == 0 && $function== 2)
    {
      save_config_data($limit, $tmp == 0 ? "yes" : $tmp == 1 ? "no" : "error",
		       $prompt);
      print "$limits{$limit}\n";
      return $function == 0 ? $limits{$limit} eq "yes" : 0;
    }
    return 1;			# more things to check
  }
  print "$limits{$limit}\n";
  return 0 if ($function);
  return $limits{$limit} eq "yes";
}


sub try_and_report
{
  my ($prompt,$limit,@tests)=@_;
  my ($tmp,$test,$type);

  print "$prompt: ";

  if (!defined($limits{$limit}))
  {
    save_incomplete($limit,$prompt);
    $type="no";			# Not supported
    foreach $test (@tests)
    {
      my $tmp_type= shift(@$test);
      if (safe_query_l($limit,\@$test))
      {
	$type=$tmp_type;
	goto outer;
      }
    }
  outer:
    save_config_data($limit, $type, $prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
}

#
# Just execute the query and check values;  Returns 1 if ok
#

sub execute_and_check
{
  my ($key,$pre,$query,$post,$answer,$string_type)=@_;
  my ($tmp);

  $tmp=safe_query_l($key,\@$pre);

  $tmp=safe_query_result_l($key,$query,$answer,$string_type) == 0 if ($tmp);
  safe_query_l($key,\@$post);
  return $tmp;
}


# returns 0 if ok, 1 if error, -1 if wrong answer
# Sets $last_result to value of query
sub safe_query_result_l{
  my ($key,$query,$answer,$result_type)=@_;
  my $r = safe_query_result($query,$answer,$result_type);
  add_log($key,$safe_query_result_log);
  return $r;
}  

sub safe_query_result
{
# result type can be 
#  8 (must be empty), 2 (Any value), 0 (number)
#  1 (char, endspaces can differ), 3 (exact char), 4 (NULL)
#  5 (char with prefix), 6 (exact, errors are ignored)
#  7 (array of numbers)
  my ($query,$answer,$result_type)=@_;
  my ($sth,$row,$result,$retry);
  undef($last_result);
  $safe_query_result_log="";
  
  printf "\nquery3: %-80.80s\n",$query  if ($opt_log_all_queries);
  print LOG "$query;\n" if ($opt_log);
  $safe_query_result_log="<".$query."\n";

  for ($retry=0; $retry < $retry_limit ; $retry++)
  {
    if (!($sth=$dbh->prepare($query)))
    {
      print_query($query);
      $safe_query_result_log .= "> prepare failed:".$dbh->errstr."\n";
      
      if ($server->abort_if_fatal_error())
      {
	check_connect();	# Check that server is still up
	next;			# Retry again
      }
      check_connect();		# Check that server is still up
      return 1;
    }
    if (!$sth->execute)
    {
      print_query($query);
      $safe_query_result_log .= "> execute failed:".$dbh->errstr."\n";
      if ($server->abort_if_fatal_error())
      {
	check_connect();	# Check that server is still up
	next;			# Retry again
      }
      check_connect();		# Check that server is still up
      return 1;
    }
    else
    {
      last;
    }
  }
  if (!($row=$sth->fetchrow_arrayref))
  {
    print "\nquery: $query didn't return any result\n" if ($opt_debug);
    $safe_query_result_log .= "> didn't return any result:".$dbh->errstr."\n";    
    $sth->finish;
    return ($result_type == 8) ? 0 : 1;
  }
  if ($result_type == 8)
  {
    $sth->finish;
    return 1;
  }
  $result=0;                  	# Ok
  $last_result= $row->[0];	# Save for report_result;
  $safe_query_result_log .= ">".$last_result."\n";    
  # Note:
  # if ($result_type == 2)        We accept any return value as answer

  if ($result_type == 0)	# Compare numbers
  {
    $row->[0] =~ s/,/./;	# Fix if ',' is used instead of '.'
    if ($row->[0] != $answer && (abs($row->[0]- $answer)/
				 (abs($row->[0]) + abs($answer))) > 0.01)
    {
      $result=-1;
      $safe_query_result_log .= 
          "We expected '$answer' but got '$last_result' \n";    
    }
  }
  elsif ($result_type == 1)	# Compare where end space may differ
  {
    $row->[0] =~ s/\s+$//;
    if ($row->[0] ne $answer)
    {
     $result=-1;
     $safe_query_result_log .= 
         "We expected '$answer' but got '$last_result' \n";    
    } ;
  }
  elsif ($result_type == 3)	# This should be a exact match
  {
     if ($row->[0] ne $answer)
     { 
      $result= -1; 
      $safe_query_result_log .= 
          "We expected '$answer' but got '$last_result' \n";    
    };
  }
  elsif ($result_type == 4)	# If results should be NULL
  {
    if (defined($row->[0]))
    { 
     $result= -1; 
     $safe_query_result_log .= 
         "We expected NULL but got '$last_result' \n";    
    };
  }
  elsif ($result_type == 5)	# Result should have given prefix
  {
     if (length($row->[0]) < length($answer) &&
		    substr($row->[0],1,length($answer)) ne $answer)
     { 
      $result= -1 ;
      $safe_query_result_log .= 
        "Result must have prefix '$answer', but  '$last_result' \n";    
     };
  }
  elsif ($result_type == 6)	# Exact match but ignore errors
  {
    if ($row->[0] ne $answer)    
    { $result= 1;
      $safe_query_result_log .= 
          "We expected '$answer' but got '$last_result' \n";    
    } ;
  }
  elsif ($result_type == 7)	# Compare against array of numbers
  {
    if ($row->[0] != $answer->[0])
    {
      $safe_query_result_log .= "must be '$answer->[0]' \n";    
      $result= -1;
    }
    else
    {
      my ($value);
      shift @$answer;
      while (($row=$sth->fetchrow_arrayref))
      {
       $safe_query_result_log .= ">$row\n";    

	$value=shift(@$answer);
	if (!defined($value))
	{
	  print "\nquery: $query returned to many results\n"
	    if ($opt_debug);
          $safe_query_result_log .= "It returned to many results \n";    	    
	  $result= 1;
	  last;
	}
	if ($row->[0] != $value)
	{
          $safe_query_result_log .= "Must return $value here \n";    	    
	  $result= -1;
	  last;
	}
      }
      if ($#$answer != -1)
      {
	print "\nquery: $query returned too few results\n"
	  if ($opt_debug);
        $safe_query_result_log .= "It returned too few results \n";    	    
	$result= 1;
      }
    }
  }
  $sth->finish;
  print "\nquery: '$query' returned '$row->[0]' instead of '$answer'\n"
    if ($opt_debug && $result && $result_type != 7);
  return $result;
}

#
# Find limit using binary search.  This is a weighed binary search that
# will prefere lower limits to get the server to crash as 
# few times as possible


sub find_limit()
{
  my ($prompt,$limit,$query)=@_;
  my ($first,$end,$i,$tmp,@tmp_array, $queries);
  print "$prompt: ";
  if (defined($end=$limits{$limit}))
  {
    print "$end (cache)\n";
    return $end;
  }
  save_incomplete($limit,$prompt);
  add_log($limit,"We are trying (example with N=5):");
  $queries = $query->query(5);
  if (ref($queries) ne "ARRAY")
  {
    push(@tmp_array,$queries);
    $queries= \@tmp_array;
  }
  foreach $tmp (@$queries)
  {   add_log($limit,repr_query($tmp));  }    

  if (defined($queries = $query->check_query()))
  { 
    if (ref($queries) ne "ARRAY")
    {
      @tmp_array=();
      push(@tmp_array,$queries); 
      $queries= \@tmp_array;
    }
    foreach $tmp (@$queries)
      {   add_log($limit,repr_query($tmp));  }    
  }
  if (defined($query->{'init'}) && !defined($end=$limits{'restart'}{'tohigh'}))
  {
    if (!safe_query_l($limit,$query->{'init'}))
    {
      $query->cleanup();
      return "error";
    }
  }

  if (!limit_query($query,1))           # This must work
  {
    print "\nMaybe fatal error: Can't check '$prompt' for limit=1\n".
    "error: $last_error\n";
    return "error";
  }

  $first=0;
  $first=$limits{'restart'}{'low'} if ($limits{'restart'}{'low'});

  if (defined($end=$limits{'restart'}{'tohigh'}))
  {
    $end--;
    print "\nRestarting this with low limit: $first and high limit: $end\n";
    delete $limits{'restart'};
    $i=$first+int(($end-$first+4)/5);           # Prefere lower on errors
  }
  else
  {
    $end= $query->max_limit();
    $i=int(($end+$first)/2);
  }
  my $log_str = "";
  unless(limit_query($query,0+$end)) {
    while ($first < $end)
    {
      print "." if ($opt_debug);
      save_config_data("restart",$i,"") if ($opt_restart);
      if (limit_query($query,$i))
      {
        $first=$i;
	$log_str .= " $i:OK";
        $i=$first+int(($end-$first+1)/2); # to be a bit faster to go up
      }
      else
      { 
        $end=$i-1;
	$log_str .= " $i:FAIL";
        $i=$first+int(($end-$first+4)/5); # Prefere lower on errors
      }
    }
  }
  $end+=$query->{'offset'} if ($end && defined($query->{'offset'}));
  if ($end >= $query->{'max_limit'} &&
      substr($query->{'max_limit'},0,1) eq '+')
  {
    $end= $query->{'max_limit'};
  }
  print "$end\n";
  add_log($limit,$log_str);
  save_config_data($limit,$end,$prompt);
  delete $limits{'restart'};
  return $end;
}

#
# Check that the query works!
#

sub assert
{
  my($query)=@_;

  if (!safe_query($query))
  {
    $query=join("; ",@$query) if (ref($query) eq "ARRAY");
    print "\nFatal error:\nquery: '$query'\nerror: $DBI::errstr\n";
    exit 1;
  }
}


sub read_config_data
{
  my ($key,$limit,$prompt);
  if (-e $opt_config_file)
  {
    open(CONFIG_FILE,"+<$opt_config_file") ||
      die "Can't open configure file $opt_config_file\n";
    print "Reading old values from cache: $opt_config_file\n";
  }
  else
  {
    open(CONFIG_FILE,"+>>$opt_config_file") ||
      die "Can't create configure file $opt_config_file: $!\n";
  }
  select CONFIG_FILE;
  $|=1;
  select STDOUT;
  while (<CONFIG_FILE>)
  {
    chomp;
    if (/^(\S+)=([^\#]*[^\#\s])\s*(\# .*)*$/)
    {
      $key=$1; $limit=$2 ; $prompt=$3;
      if (!$opt_quick || $limit =~ /\d/ || $key =~ /crash_me/)
      {
	if ($key !~ /restart/i)
	{
	  $limits{$key}=$limit eq "null"? undef : $limit;
	  $prompts{$key}=length($prompt) ? substr($prompt,2) : "";
	  $last_read=$key;
	  delete $limits{'restart'};
	}
	else
	{
	  $limit_changed=1;
	  if ($limit > $limits{'restart'}{'tohigh'})
	  {
	    $limits{'restart'}{'low'} = $limits{'restart'}{'tohigh'};
	  }
	  $limits{'restart'}{'tohigh'} = $limit;
	}
      }
    }
    elsif (/\s*###(.*)$/)    # log line
    {
       # add log line for previously read key
       $log{$last_read} .= "$1\n";
    }
    elsif (!/^\s*$/ && !/^\#/)
    {
      die "Wrong config row: $_\n";
    }
  }
}


sub save_config_data
{
  my ($key,$limit,$prompt)=@_;
  $prompts{$key}=$prompt;
  return if (defined($limits{$key}) && $limits{$key} eq $limit);
  if (!defined($limit) || $limit eq "")
  {
#    die "Undefined limit for $key\n";
     $limit = 'null'; 
  }
  print CONFIG_FILE "$key=$limit\t# $prompt\n";
  $limits{$key}=$limit;
  $limit_changed=1;
# now write log lines (immediatelly after limits)
  my $line;
  my $last_line_was_empty=0;
  foreach $line (split /\n/, $log{$key})
  {
    print CONFIG_FILE "   ###$line\n" 
	unless ( ($last_line_was_empty eq 1)  
	         && ($line =~ /^\s+$/)  );
    $last_line_was_empty= ($line =~ /^\s+$/)?1:0;
  };     

  if (($opt_restart && $limits{'operating_system'} =~ /windows/i) ||
		       ($limits{'operating_system'} =~ /NT/))
  {
    # If perl crashes in windows, everything is lost (Wonder why? :)
    close CONFIG_FILE;
    open(CONFIG_FILE,"+>>$opt_config_file") ||
      die "Can't reopen configure file $opt_config_file: $!\n";
  }
}

sub add_log
{
  my $key = shift;
  my $line = shift;
  $log{$key} .= $line . "\n" if ($opt_verbose);;  
}

sub save_all_config_data
{
  my ($key,$tmp);
  close CONFIG_FILE;
  return if (!$limit_changed);
  open(CONFIG_FILE,">$opt_config_file") ||
    die "Can't create configure file $opt_config_file: $!\n";
  select CONFIG_FILE;
  $|=1;
  select STDOUT;
  delete $limits{'restart'};

  print CONFIG_FILE 
       "#This file is automaticly generated by crash-me $version\n\n";
  foreach $key (sort keys %limits)
  {
    $tmp="$key=$limits{$key}";
    print CONFIG_FILE $tmp . ("\t" x (int((32-min(length($tmp),32)+7)/8)+1)) .
      "# $prompts{$key}\n";
     my $line;
     my $last_line_was_empty=0;
     foreach $line (split /\n/, $log{$key})
     {
        print CONFIG_FILE "   ###$line\n" unless 
	      ( ($last_line_was_empty eq 1) && ($line =~ /^\s*$/));
        $last_line_was_empty= ($line =~ /^\s*$/)?1:0;
     };     
  }
  close CONFIG_FILE;
}

#
# Save 'incomplete' in the limits file to be able to continue if
# crash-me dies because of a bug in perl/DBI

sub save_incomplete
{
  my ($limit,$prompt)= @_;
  save_config_data($limit,"incompleted",$prompt) if ($opt_restart);
}


sub check_repeat
{
  my ($sth,$limit)=@_;
  my ($row);

  return 0 if (!($row=$sth->fetchrow_arrayref));
  return (defined($row->[0]) && ('a' x $limit) eq $row->[0]) ? 1 : 0;
}


sub min
{
  my($min)=$_[0];
  my($i);
  for ($i=1 ; $i <= $#_; $i++)
  {
    $min=$_[$i] if ($min > $_[$i]);
  }
  return $min;
}

sub sql_concat
{
  my ($a,$b)= @_;
  return "$a || $b" if ($limits{'func_sql_concat_as_||'} eq 'yes');
  return "concat($a,$b)" if ($limits{'func_odbc_concat'} eq 'yes');
  return "$a + $b" if ($limits{'func_extra_concat_as_+'} eq 'yes');
  return undef;
}

#
# Returns a list of statements to create a table in a portable manner
# but still utilizing features in the databases.
#

sub create_table
{
  my($table_name,$fields,$index,$extra) = @_;
  my($query,$nr,$parts,@queries,@index);

  $extra="" if (!defined($extra));

  $query="create table $table_name (";
  $nr=0;
  foreach $field (@$fields)
  {
    $query.= $field . ',';
  }
  foreach $index (@$index)
  {
    $index =~ /\(([^\(]*)\)$/i;
    $parts=$1;
    if ($index =~ /^primary key/)
    {
      if ($limits{'primary_key_in_create'} eq 'yes')
      {
	$query.= $index . ',';
      }
      else
      {
	push(@queries,
	     "create unique index ${table_name}_prim on $table_name ($parts)");
      }
    }
    elsif ($index =~ /^unique/)
    {
      if ($limits{'unique_in_create'} eq 'yes')
      {
	$query.= "unique ($parts),";
      }
      else
      {
	$nr++;
	push(@queries,
	     "create unique index ${table_name}_$nr on $table_name ($parts)");

      }
    }
    else
    {
      if ($limits{'index_in_create'} eq 'yes')
      {
	$query.= "index ($parts),";
      }
      else
      {
	$nr++;
	push(@queries,
	     "create index ${table_name}_$nr on $table_name ($1)");
      }
    }
  }
  chop($query);
  $query.= ") $extra";
  unshift(@queries,$query);
  return @queries;
}


#
# This is used by some query packages to change:
# %d -> limit
# %s -> 'a' x limit
# %v -> "1,1,1,1,1" where there are 'limit' number of ones
# %f -> q1,q2,q3....
# %F -> q1 integer,q2 integer,q3 integer....

sub fix_query
{
  my ($query,$limit)=@_;
  my ($repeat,$i);

  return $query if !(defined($query));
  $query =~ s/%d/$limit/g;
  if ($query =~ /%s/)
  {
    $repeat= 'a' x $limit;
    $query =~ s/%s/$repeat/g;
  }
  if ($query =~ /%v/)
  {
    $repeat= '1,' x $limit;
    chop($repeat);
    $query =~ s/%v/$repeat/g;
  }
  if ($query =~ /%f/)
  {
    $repeat="";
    for ($i=1 ; $i <= $limit ; $i++)
    {
      $repeat.="q$i,";
    }
    chop($repeat);
    $query =~ s/%f/$repeat/g;
  }
  if ($query =~ /%F/)
  {
    $repeat="";
    for ($i=1 ; $i <= $limit ; $i++)
    {
      $repeat.="q$i integer,";
    }
    chop($repeat);
    $query =~ s/%F/$repeat/g;
  }
  return $query;
}


#
# Different query packages
#

package query_repeat;

sub new
{
  my ($type,$init,$query,$add1,$add_mid,$add,$add_end,$end_query,$cleanup,
      $max_limit, $check, $offset)=@_;
  my $self={};
  if (defined($init) && $#$init != -1)
  {
    $self->{'init'}=$init;
  }
  $self->{'query'}=$query;
  $self->{'add1'}=$add1;
  $self->{'add_mid'}=$add_mid;
  $self->{'add'}=$add;
  $self->{'add_end'}=$add_end;
  $self->{'end_query'}=$end_query;
  $self->{'cleanup'}=$cleanup;
  $self->{'max_limit'}=(defined($max_limit) ? $max_limit : $main::query_size);
  $self->{'check'}=$check;
  $self->{'offset'}=$offset;
  $self->{'printf'}= ($add =~ /%d/);
  bless $self;
}

sub query
{
  my ($self,$limit)=@_;
  if (!$self->{'printf'})
  {
    return $self->{'query'} . ($self->{'add'} x $limit) .
      ($self->{'add_end'} x $limit) . $self->{'end_query'};
  }
  my ($tmp,$tmp2,$tmp3,$i);
  $tmp=$self->{'query'};
  if ($self->{'add1'})
  {
    for ($i=0; $i < $limit ; $i++)
    {
      $tmp3 = $self->{'add1'};
      $tmp3 =~ s/%d/$i/g;
      $tmp  .= $tmp3;
    }
  }
  $tmp .= " ".$self->{'add_mid'};
  if ($self->{'add'})
  {
    for ($i=0; $i < $limit ; $i++)
    {
      $tmp2 = $self->{'add'};
      $tmp2 =~ s/%d/$i/g;
      $tmp  .= $tmp2;
    }
  }
  return ($tmp .
	  ($self->{'add_end'} x $limit) . $self->{'end_query'});
}

sub max_limit
{
  my ($self)=@_;
  my $tmp;
  $tmp=int(($main::limits{"query_size"}-length($self->{'query'})
	    -length($self->{'add_mid'})-length($self->{'end_query'}))/
	   (length($self->{'add1'})+
	   length($self->{'add'})+length($self->{'add_end'})));
  return main::min($self->{'max_limit'},$tmp);
}


sub cleanup
{
  my ($self)=@_;
  my($tmp,$statement);
  $tmp=$self->{'cleanup'};
  foreach $statement (@$tmp)
  {
    main::safe_query($statement) if (defined($statement) && length($statement));
  }
}

sub check
{
  my ($self,$sth)=@_;
  my $check=$self->{'check'};
  return &$check($sth,$self->{'limit'}) if (defined($check));
  return 1;
}

sub check_query
{
  return undef;
}


package query_num;

sub new
{
  my ($type,$query,$end_query,$cleanup,$max_limit,$check)=@_;
  my $self={};
  $self->{'query'}=$query;
  $self->{'end_query'}=$end_query;
  $self->{'cleanup'}=$cleanup;
  $self->{'max_limit'}=$max_limit;
  $self->{'check'}=$check;
  bless $self;
}

sub query
{
  my ($self,$i)=@_;
  $self->{'limit'}=$i;
  return "$self->{'query'}$i$self->{'end_query'}";
}

sub max_limit
{
  my ($self)=@_;
  return $self->{'max_limit'};
}

sub cleanup
{
  my ($self)=@_;
  my($statement);
  foreach $statement ($self->{'$cleanup'})
  {
    main::safe_query($statement) if (defined($statement) && length($statement));
  }
}


sub check
{
  my ($self,$sth)=@_;
  my $check=$self->{'check'};
  return &$check($sth,$self->{'limit'}) if (defined($check));
  return 1;
}

sub check_query
{
  return undef;
}

#
# This package is used when testing CREATE TABLE!
#

package query_table;

sub new
{
  my ($type,$query, $add, $end_query, $extra_init, $safe_query, $check,
      $cleanup, $max_limit, $offset)=@_;
  my $self={};
  $self->{'query'}=$query;
  $self->{'add'}=$add;
  $self->{'end_query'}=$end_query;
  $self->{'extra_init'}=$extra_init;
  $self->{'safe_query'}=$safe_query;
  $self->{'check'}=$check;
  $self->{'cleanup'}=$cleanup;
  $self->{'max_limit'}=$max_limit;
  $self->{'offset'}=$offset;
  bless $self;
}


sub query
{
  my ($self,$limit)=@_;
  $self->{'limit'}=$limit;
  $self->cleanup();     # Drop table before create

  my ($tmp,$tmp2,$i,$query,@res);
  $tmp =$self->{'query'};
  $tmp =~ s/%d/$limit/g;
  for ($i=1; $i <= $limit ; $i++)
  {
    $tmp2 = $self->{'add'};
    $tmp2 =~ s/%d/$i/g;
    $tmp  .= $tmp2;
  }
  push(@res,$tmp . $self->{'end_query'});
  $tmp=$self->{'extra_init'};
  foreach $query (@$tmp)
  {
    push(@res,main::fix_query($query,$limit));
  }
  return \@res;
}


sub max_limit
{
  my ($self)=@_;
  return $self->{'max_limit'};
}


sub check_query
{
  my ($self)=@_;
  return main::fix_query($self->{'safe_query'},$self->{'limit'});
}

sub check
{
  my ($self,$sth)=@_;
  my $check=$self->{'check'};
  return 0 if (!($row=$sth->fetchrow_arrayref));
  if (defined($check))
  {
    return (defined($row->[0]) &&
	    $row->[0] eq main::fix_query($check,$self->{'limit'})) ? 1 : 0;
  }
  return 1;
}


# Remove table before and after create table query

sub cleanup()
{
  my ($self)=@_;
  main::safe_query(main::fix_query($self->{'cleanup'},$self->{'limit'}));
}

#
# Package to do many queries with %d, and %s substitution
#

package query_many;

sub new
{
  my ($type,$query,$safe_query,$check_result,$cleanup,$max_limit,$offset,
      $safe_cleanup)=@_;
  my $self={};
  $self->{'query'}=$query;
  $self->{'safe_query'}=$safe_query;
  $self->{'check'}=$check_result;
  $self->{'cleanup'}=$cleanup;
  $self->{'max_limit'}=$max_limit;
  $self->{'offset'}=$offset;
  $self->{'safe_cleanup'}=$safe_cleanup;
  bless $self;
}


sub query
{
  my ($self,$limit)=@_;
  my ($queries,$query,@res);
  $self->{'limit'}=$limit;
  $self->cleanup() if (defined($self->{'safe_cleanup'}));
  $queries=$self->{'query'};
  foreach $query (@$queries)
  {
    push(@res,main::fix_query($query,$limit));
  }
  return \@res;
}

sub check_query
{
  my ($self)=@_;
  return main::fix_query($self->{'safe_query'},$self->{'limit'});
}

sub cleanup
{
  my ($self)=@_;
  my($tmp,$statement);
  return if (!defined($self->{'cleanup'}));
  $tmp=$self->{'cleanup'};
  foreach $statement (@$tmp)
  {
    if (defined($statement) && length($statement))
    {
      main::safe_query(main::fix_query($statement,$self->{'limit'}));
    }
  }
}


sub check
{
  my ($self,$sth)=@_;
  my ($check,$row);
  return 0 if (!($row=$sth->fetchrow_arrayref));
  $check=$self->{'check'};
  if (defined($check))
  {
    return (defined($row->[0]) &&
	    $row->[0] eq main::fix_query($check,$self->{'limit'})) ? 1 : 0;
  }
  return 1;
}

sub max_limit
{
  my ($self)=@_;
  return $self->{'max_limit'};
}

#
# Used to find max supported row length
#

package query_row_length;

sub new
{
  my ($type,$create,$null,$drop,$max_limit)=@_;
  my $self={};
  $self->{'table_name'}=$create;
  $self->{'null'}=$null;
  $self->{'cleanup'}=$drop;
  $self->{'max_limit'}=$max_limit;
  bless $self;
}


sub query
{
  my ($self,$limit)=@_;
  my ($res,$values,$size,$length,$i);
  $self->{'limit'}=$limit;

  $res="";
  $size=main::min($main::limits{'max_char_size'},255);
  $size = 255 if (!$size); # Safety
  for ($length=$i=0; $length + $size <= $limit ; $length+=$size, $i++)
  {
    $res.= "q$i char($size) $self->{'null'},";
    $values.="'" . ('a' x $size) . "',";
  }
  if ($length < $limit)
  {
    $size=$limit-$length;
    $res.= "q$i char($size) $self->{'null'},";
    $values.="'" . ('a' x $size) . "',";
  }
  chop($res);
  chop($values);
  return ["create table " . $self->{'table_name'} . " ($res)",
	  "insert into " . $self->{'table_name'} . " values ($values)"];
}

sub max_limit
{
  my ($self)=@_;
  return $self->{'max_limit'};
}

sub cleanup
{
  my ($self)=@_;
  main::safe_query($self->{'cleanup'});
}


sub check
{
  return 1;
}

sub check_query
{
  return undef;
}

#
# Used to find max supported index length
#

package query_index_length;

sub new
{
  my ($type,$create,$drop,$max_limit)=@_;
  my $self={};
  $self->{'create'}=$create;
  $self->{'cleanup'}=$drop;
  $self->{'max_limit'}=$max_limit;
  bless $self;
}


sub query
{
  my ($self,$limit)=@_;
  my ($res,$size,$length,$i,$parts,$values);
  $self->{'limit'}=$limit;

  $res=$parts=$values="";
  $size=main::min($main::limits{'max_index_part_length'},
       $main::limits{'max_char_size'});
  $size=1 if ($size == 0);	# Avoid infinite loop errors
  for ($length=$i=0; $length + $size <= $limit ; $length+=$size, $i++)
  {
    $res.= "q$i char($size) not null,";
    $parts.= "q$i,";
    $values.= "'" . ('a' x $size) . "',";
  }
  if ($length < $limit)
  {
    $size=$limit-$length;
    $res.= "q$i char($size) not null,";
    $parts.="q$i,";
    $values.= "'" . ('a' x $size) . "',";
  }
  chop($parts);
  chop($res);
  chop($values);
  if ($main::limits{'unique_in_create'} eq 'yes')
  {
    return [$self->{'create'} . "($res,unique ($parts))",
	    "insert into crash_q values($values)"];
  }
  return [$self->{'create'} . "($res)",
	  "create index crash_q_index on crash_q ($parts)",
	  "insert into crash_q values($values)"];
}

sub max_limit
{
  my ($self)=@_;
  return $self->{'max_limit'};
}

sub cleanup
{
  my ($self)=@_;
  main::safe_query($self->{'cleanup'});
}


sub check
{
  return 1;
}

sub check_query
{
  return undef;
}



### TODO:
# OID test instead of / in addition to _rowid
