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


$version="1.48";

use DBI;
use Getopt::Long;
chomp($pwd = `pwd`); $pwd = "." if ($pwd eq '');
require "$pwd/server-cfg" || die "Can't read Configuration file: $!\n";

$opt_server="mysql"; $opt_host="localhost"; $opt_database="test";
$opt_dir="limits";
$opt_debug=$opt_help=$opt_Information=$opt_restart=$opt_force=$opt_quick=0;
$opt_log_all_queries=$opt_fix_limit_file=$opt_batch_mode=0;
$opt_db_start_cmd="";           # the db server start command
$opt_sleep=10;                  # time to sleep while starting the db server
$limit_changed=0;               # For configure file
$reconnect_count=0;
$opt_comment=$opt_config_file=$opt_log_queries_to_file="";
$limits{'crash_me_safe'}='yes';
$prompts{'crash_me_safe'}='crash me safe';
$limits{'operating_system'}= machine();
$prompts{'operating_system'}='crash-me tested on';
$retry_limit=3;

GetOptions("Information","help","server=s","debug","user=s","password=s","database=s","restart","force","quick","log-all-queries","comment=s","host=s","fix-limit-file","dir=s","db-start-cmd=s","sleep=s","batch-mode","config-file=s","log-queries-to-file=s") || usage();
usage() if ($opt_help || $opt_Information);

$opt_config_file="$pwd/$opt_dir/$opt_server.cfg" if (length($opt_config_file) == 0);

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
  open(LOG,">$opt_log_queries_to_file") || die "Can't open file $opt_log_queries_to_file\n";
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
$SIG{SEGV} = sub {warn('SEGFAULT')};
$dbh=safe_connect();
$dbh->do("drop table crash_me");        # Remove old run
$dbh->do("drop table crash_me2");        # Remove old run
$dbh->do("drop table crash_me3");        # Remove old run
$dbh->do("drop table crash_q");         # Remove old run
$dbh->do("drop table crash_q1");         # Remove old run

$prompt="Tables without primary key";
if (!safe_query(["create table crash_me (a integer not null,b char(10) not null)",
		 "insert into crash_me (a,b) values (1,'a')"]))
{
  if (!safe_query(["create table crash_me (a integer not null,b char(10) not null, primary key (a))",
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

report("Column alias","column_alias","select a as ab from crash_me");
report("Table alias","table_alias","select b.a from crash_me as b");
report("Functions",'functions',"select 1+1 $end_query");
report("Group functions",'group_functions',"select count(*) from crash_me");
report("Group functions with distinct",'group_distinct_functions',
       "select count(distinct a) from crash_me");
report("Group by",'group_by',"select a from crash_me group by a");
report("Group by position",'group_by_position',
       "select a from crash_me group by 1");
report("Group by alias",'group_by_alias',
       "select a as ab from crash_me group by ab");
report("Order by",'order_by',"select a from crash_me order by a");
report("Order by position",'order_by_position',
       "select a from crash_me order by 1");
report("Order by function","order_by_function",
       "select a from crash_me order by a+1");
check_and_report("Order by DESC is remembered",'order_by_remember_desc',
		 ["create table crash_q (s int,s1 int)",
		  "insert into crash_q values(1,1)",
		  "insert into crash_q values(3,1)",
		  "insert into crash_q values(2,1)"],
		 "select s,s1 from crash_q order by s1 DESC,s",
		 ["drop table crash_q"],[3,2,1],7,undef(),3);
report("Compute",'compute',
       "select a from crash_me order by a compute sum(a) by a");
report("Value lists in INSERT",'multi_value_insert',
       "create table crash_q (s char(10))",
       "insert into crash_q values ('a'),('b')",
       "drop table crash_q");
report("INSERT with set syntax",'insert_with_set',
       "create table crash_q (a integer)",
       "insert into crash_q SET a=1",
       "drop table crash_q");
report("allows end ';'","end_colon", "select * from crash_me;");
try_and_report("LIMIT number of rows","select_limit",
	       ["with LIMIT",
		"select * from crash_me limit 1"],
	       ["with TOP",
		"select TOP 1 * from crash_me"]);
report("SELECT with LIMIT #,#","select_limit2", "select * from crash_me limit 1,1");

# The following alter table commands MUST be kept together!
if ($dbh->do("create table crash_q (a integer, b integer,c CHAR(10))"))
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
	     [["alter table crash_q modify c CHAR(20)","yes"],
	      ["alter table crash_q alter c CHAR(20)","with alter"]]);
  report("Alter table alter column default",'alter_alter_col',
	 "alter table crash_q alter b set default 10",
	 "alter table crash_q alter b set default NULL");
  report("Alter table drop column",'alter_drop_col',
	 "alter table crash_q drop column b");
  report("Alter table rename table",'alter_rename_table',
	 "alter table crash_q rename to crash_q1");
}
# Make sure both tables will be dropped, even if rename fails.
$dbh->do("drop table crash_q1");
$dbh->do("drop table crash_q");

report("rename table","rename_table",
       "create table crash_q (a integer, b integer,c CHAR(10))",
       "rename table crash_q to crash_q1",
       "drop table crash_q1");
# Make sure both tables will be dropped, even if rename fails.
$dbh->do("drop table crash_q1");
$dbh->do("drop table crash_q");

if ($dbh->do("create table crash_q (a integer, b integer,c CHAR(10))") &&
    $dbh->do("create table crash_q1 (a integer, b integer,c CHAR(10) not null)"))
{
  report("Alter table add constraint",'alter_add_constraint',
	 "alter table crash_q add constraint c1 check(a > b)");
  report("Alter table drop constraint",'alter_drop_constraint',
	 "alter table crash_q drop constraint c1");
  report("Alter table add unique",'alter_add_unique',
	 "alter table crash_q add constraint u1 unique(c)");
  try_and_report("Alter table drop unique",'alter_drop_unique',
		 ["with constraint",
		  "alter table crash_q drop constraint u1"],
		 ["with drop key",
		  "alter table crash_q drop key c"]);
  try_and_report("Alter table add primary key",'alter_add_primary_key',
		 ["with constraint",
		  "alter table crash_q1 add constraint p1 primary key(c)"],
		 ["with add primary key",
		  "alter table crash_q1 add primary key(c)"]);
  report("Alter table add foreign key",'alter_add_foreign_key',
	 "alter table crash_q add constraint f1 foreign key(c) references crash_q1(c)");
  try_and_report("Alter table drop foreign key",'alter_drop_foreign_key',
		 ["with drop constraint",
		  "alter table crash_q drop constraint f1"],
		 ["with drop foreign key",
		  "alter table crash_q drop foreign key f1"]);
  try_and_report("Alter table drop primary key",'alter_drop_primary_key',
		 ["drop constraint",
		  "alter table crash_q1 drop constraint p1 restrict"],
		 ["drop primary key",
		  "alter table crash_q1 drop primary key"]);
}
$dbh->do("drop table crash_q");
$dbh->do("drop table crash_q1");

check_and_report("case insensitive compare","case_insensitive_strings",
		 [],"select b from crash_me where b = 'A'",[],'a',1);
check_and_report("ignore end space in compare","ignore_end_space",
		 [],"select b from crash_me where b = 'a '",[],'a',1);
check_and_report("group on column with null values",'group_by_null',
		 ["create table crash_q (s char(10))",
		  "insert into crash_q values(null)",
		  "insert into crash_q values(null)"],
		 "select count(*) from crash_q group by s",
		 ["drop table crash_q"],2,0);

$prompt="Having";
if (!defined($limits{'having'}))
{                               # Complicated because of postgreSQL
  if (!safe_query_result("select a from crash_me group by a having a > 0",1,0))
  {
    if (!safe_query_result("select a from crash_me group by a having a < 0",
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

$logical_value= $limits{'logical_value'};

$false=0;
$result="no";
if ($res=safe_query("select (1=1)=true $end_query")) {
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
    print "Can't connect to server: $DBI::errstr.  Please start it and try again\n";
    exit 1;
  }
  $dbh=safe_connect();
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
    last if (!safe_query($query . (" " x ($i - length($query)-length($end_query) -1)) . "$select$end_query"));
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
            "interval hour", "interval hour to minute", "interval hour to second",
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
	      "interval", "serial", "inet", "cidr", "macaddr",

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
	   "drop table crash_q");
  }
}

#
# Test some type limits
#

check_and_report("Remembers end space in char()","remember_end_space",
		 ["create table crash_q (a char(10))",
		  "insert into crash_q values('hello ')"],
		 "select a from crash_q where a = 'hello '",
		 ["drop table crash_q"],
		 'hello ',6);

check_and_report("Remembers end space in varchar()",
		 "remember_end_space_varchar",
		 ["create table crash_q (a varchar(10))",
		  "insert into crash_q values('hello ')"],
		 "select a from crash_q where a = 'hello '",
		 ["drop table crash_q"],
		 'hello ',6);

check_and_report("Supports 0000-00-00 dates","date_zero",
		 ["create table crash_me2 (a date not null)",
		  "insert into crash_me2 values ('0000-00-00')"],
		 "select a from crash_me2",
		 ["drop table crash_me2"],
		 "0000-00-00",1);

check_and_report("Supports 0001-01-01 dates","date_one",
		 ["create table crash_me2 (a date not null)",
		  "insert into crash_me2 values (DATE '0001-01-01')"],
		 "select a from crash_me2",
		 ["drop table crash_me2"],
		 "0001-01-01",1);

check_and_report("Supports 9999-12-31 dates","date_last",
		 ["create table crash_me2 (a date not null)",
		  "insert into crash_me2 values (DATE '9999-12-31')"],
		 "select a from crash_me2",
		 ["drop table crash_me2"],
		 "9999-12-31",1);

check_and_report("Supports 'infinity dates","date_infinity",
		 ["create table crash_me2 (a date not null)",
		  "insert into crash_me2 values ('infinity')"],
		 "select a from crash_me2",
		 ["drop table crash_me2"],
		 "infinity",1);

if (!defined($limits{'date_with_YY'}))
{
    check_and_report("Supports YY-MM-DD dates","date_with_YY",
		     ["create table crash_me2 (a date not null)",
		      "insert into crash_me2 values ('98-03-03')"],
		     "select a from crash_me2",
		     ["drop table crash_me2"],
		     "1998-03-03",5);
    if ($limits{'date_with_YY'} eq "yes")
    {
	undef($limits{'date_with_YY'});
	check_and_report("Supports YY-MM-DD 2000 compilant dates",
			 "date_with_YY",
			 ["create table crash_me2 (a date not null)",
			  "insert into crash_me2 values ('10-03-03')"],
			 "select a from crash_me2",
			 ["drop table crash_me2"],
			 "2010-03-03",5);
    }
}

if (($limits{'type_extra_float(2_arg)'} eq "yes" ||
    $limits{'type_sql_decimal(2_arg)'} eq "yes") &&
    (!defined($limits{'storage_of_float'})))
{
  my $type=$limits{'type_extra_float(2_arg)'} eq "yes" ? "float(4,1)" :
    "decimal(4,1)";
  my $result="undefined";
  if (execute_and_check(["create table crash_q (q1 $type)",
			 "insert into crash_q values(1.14)"],
			"select q1 from crash_q",
			["drop table crash_q"],1.1,0) &&
      execute_and_check(["create table crash_q (q1 $type)",
			 "insert into crash_q values(1.16)"],
			"select q1 from crash_q",
			["drop table crash_q"],1.1,0))
  {
    $result="truncate";
  }
  elsif (execute_and_check(["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.14)"],
			   "select q1 from crash_q",
			   ["drop table crash_q"],1.1,0) &&
	 execute_and_check(["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.16)"],
			   "select q1 from crash_q",
			   ["drop table crash_q"],1.2,0))
  {
    $result="round";
  }
  elsif (execute_and_check(["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.14)"],
			   "select q1 from crash_q",
			   ["drop table crash_q"],1.14,0) &&
	 execute_and_check(["create table crash_q (q1 $type)",
			    "insert into crash_q values(1.16)"],
			   "select q1 from crash_q",
			   ["drop table crash_q"],1.16,0))
  {
    $result="exact";
  }
  $prompt="Storage of float values";
  print "$prompt: $result\n";
  save_config_data("storage_of_float", $result, $prompt);
}

try_and_report("Type for row id", "rowid",
	       ["rowid",
		"create table crash_q (a rowid)","drop table crash_q"],
	       ["auto_increment",
		"create table crash_q (a int not null auto_increment, primary key(a))","drop table crash_q"],
	       ["oid",
		"create table crash_q (a oid, primary key(a))","drop table crash_q"],
	       ["serial",
		"create table crash_q (a serial, primary key(a))","drop table crash_q"]);

try_and_report("Automatic rowid", "automatic_rowid",
	       ["_rowid",
		"create table crash_q (a int not null, primary key(a))",
		"insert into crash_q values (1)",
		"select _rowid from crash_q",
		"drop table crash_q"]);

#
# Test functions
#

@sql_functions=
  (["+, -, * and /","+","5*3-4/2+1",14,0],
   ["ANSI SQL SUBSTRING","substring","substring('abcd' from 2 for 2)","bc",1],
   ["BIT_LENGTH","bit_length","bit_length('abc')",24,0],
   ["searched CASE","searched_case","case when 1 > 2 then 'false' when 2 > 1 then 'true' end", "true",1],
   ["simple CASE","simple_case","case 2 when 1 then 'false' when 2 then 'true' end", "true",1],
   ["CAST","cast","CAST(1 as CHAR)","1",1],
   ["CHARACTER_LENGTH","character_length","character_length('abcd')","4",0],
   ["CHAR_LENGTH","char_length","char_length(b)","10",0],
   ["CHAR_LENGTH(constant)","char_length(constant)","char_length('abcd')","4",0],
   ["COALESCE","coalesce","coalesce($char_null,'bcd','qwe')","bcd",1],
   ["CURRENT_DATE","current_date","current_date",0,2],
   ["CURRENT_TIME","current_time","current_time",0,2],
   ["CURRENT_TIMESTAMP","current_timestamp","current_timestamp",0,2],
   ["CURRENT_USER","current_user","current_user",0,2],
   ["EXTRACT","extract_sql","extract(minute from timestamp '2000-02-23 18:43:12.987')",43,0],
   ["LOCALTIME","localtime","localtime",0,2],
   ["LOCALTIMESTAMP","localtimestamp","localtimestamp",0,2],
   ["LOWER","lower","LOWER('ABC')","abc",1],
   ["NULLIF with strings","nullif_string","NULLIF(NULLIF('first','second'),'first')",undef(),4],
   ["NULLIF with numbers","nullif_num","NULLIF(NULLIF(1,2),1)",undef(),4],
   ["OCTET_LENGTH","octet_length","octet_length('abc')",3,0],
   ["POSITION","position","position('ll' in 'hello')",3,0],
   ["SESSION_USER","session_user","session_user",0,2],
   ["SYSTEM_USER","system_user","system_user",0,2],
   ["TRIM","trim","trim(trailing from trim(LEADING FROM ' abc '))","abc",3],
   ["UPPER","upper","UPPER('abc')","ABC",1],
   ["USER","user","user"],
   ["concatenation with ||","concat_as_||","'abc' || 'def'","abcdef",1],
   );

@odbc_functions=
  (["ASCII", "ascii", "ASCII('A')","65",0],
   ["CHAR", "char", "CHAR(65)"  ,"A",1],
   ["CONCAT(2 arg)","concat", "concat('a','b')","ab",1],
   ["DIFFERENCE()","difference","difference('abc','abe')",0,2],
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
   ["EXP","exp","exp(1)","2.718282",0],
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
   ["DAYNAME","dayname","dayname(DATE '1997-02-01')","",2],
   ["MONTH","month","month(DATE '1997-02-01')","",2],
   ["MONTHNAME","monthname","monthname(DATE '1997-02-01')","",2],
   ["DAYOFMONTH","dayofmonth","dayofmonth(DATE '1997-02-01')",1,0],
   ["DAYOFWEEK","dayofweek","dayofweek(DATE '1997-02-01')",7,0],
   ["DAYOFYEAR","dayofyear","dayofyear(DATE '1997-02-01')",32,0],
   ["QUARTER","quarter","quarter(DATE '1997-02-01')",1,0],
   ["WEEK","week","week(DATE '1997-02-01')",5,0],
   ["YEAR","year","year(DATE '1997-02-01')",1997,0],
   ["CURTIME","curtime","curtime()",0,2],
   ["HOUR","hour","hour('12:13:14')",12,0],
   ["ANSI HOUR","hour_time","hour(TIME '12:13:14')",12,0],
   ["MINUTE","minute","minute('12:13:14')",13,0],
   ["SECOND","second","second('12:13:14')",14,0],
   ["TIMESTAMPADD","timestampadd",
    "timestampadd(SQL_TSI_SECOND,1,'1997-01-01 00:00:00')",
    "1997-01-01 00:00:01",1],
   ["TIMESTAMPDIFF","timestampdiff",
    "timestampdiff(SQL_TSI_SECOND,'1997-01-01 00:00:02', '1997-01-01 00:00:01')","1",0],
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
   ["ADD_MONTHS","add_months","add_months('1997-01-01',1)","1997-02-01",0], # oracle the date plus n months
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
   ["DATEADD","dateadd","dateadd(day,3,'Nov 30 1997')",0,2],
   ["DATEDIFF","datediff","datediff(month,'Oct 21 1997','Nov 30 1997')",0,2],
   ["DATENAME","datename","datename(month,'Nov 30 1997')",0,2],
   ["DATEPART","datepart","datepart(month,'July 20 1997')",0,2],
   ["DATE_FORMAT","date_format", "date_format('1997-01-02 03:04:05','M W D Y y m d h i s w')", 0,2],
   ["ELT","elt","elt(2,'ONE','TWO','THREE')","TWO",1],
   ["ENCRYPT","encrypt","encrypt('hello')",0,2],
   ["FIELD","field","field('IBM','NCA','ICL','SUN','IBM','DIGITAL')",4,0],
   ["FORMAT","format","format(1234.5555,2)","1,234.56",1],
   ["FROM_DAYS","from_days","from_days(729024)","1996-01-01",1],
   ["FROM_UNIXTIME","from_unixtime","from_unixtime(0)",0,2],
   ["GETDATE","getdate","getdate()",0,2],
   ["GREATEST","greatest","greatest('HARRY','HARRIOT','HAROLD')","HARRY",1], # oracle
   ["IF","if", "if(5,6,7)",6,0],
   ["IN on numbers in SELECT","in_num","2 in (3,2,5,9,5,1)",$logical_value,0],
   ["IN on strings in SELECT","in_str","'monty' in ('david','monty','allan')", $logical_value,0],
   ["INITCAP","initcap","initcap('the soap')","The Soap",1], # oracle Returns char, with the first letter of each word in uppercase
   ["INSTR (Oracle syntax)", "instr_oracle", "INSTR('CORPORATE FLOOR','OR',3,2)"  ,"14",0], # oracle instring
   ["INSTRB", "instrb", "INSTRB('CORPORATE FLOOR','OR',5,2)"  ,"27",0], # oracle instring in bytes
   ["INTERVAL","interval","interval(55,10,20,30,40,50,60,70,80,90,100)",5,0],
   ["LAST_DAY","last_day","last_day('1997-04-01')","1997-04-30",0], # oracle last day of month of date
   ["LAST_INSERT_ID","last_insert_id","last_insert_id()",0,2],
   ["LEAST","least","least('HARRY','HARRIOT','HAROLD')","HAROLD",1], # oracle
   ["LENGTHB","lengthb","lengthb('CANDIDE')","14",0], # oracle length in bytes
   ["LIKE ESCAPE in SELECT","like_escape","'%' like 'a%' escape 'a'",$logical_value,0],
   ["LIKE in SELECT","like","'a' like 'a%'",$logical_value,0],
   ["LN","ln","ln(95)","4.55387689",0], # oracle natural logarithm of n
   ["LOCATE as INSTR","instr","instr('hello','ll')",3,0],
   ["LOG(m,n)","log(m_n)","log(10,100)","2",0], # oracle logarithm, base m, of n
   ["LOGN","logn","logn(2)","0.693147",0], # informix
   ["LPAD","lpad","lpad('hi',4,'??')",'??hi',3],
   ["MDY","mdy","mdy(7,1,1998)","1998-07-01",0], # informix
   ["MOD as %","%","10%7","3",0],
   ["MONTHS_BETWEEN","months_between","months_between('1997-02-02','1997-01-01')","1.03225806",0], # oracle number of months between 2 dates
   ["NOT BETWEEN in SELECT","not_between","5 not between 4 and 6",0,0],
   ["NOT LIKE in SELECT","not_like","'a' not like 'a%'",0,0],
   ["NOT as '!' in SELECT","!","! 1",0,0],
   ["NOT in SELECT","not","not $false",$logical_value,0],
   ["ODBC CONVERT","odbc_convert","convert(5,SQL_CHAR)","5",1],
   ["OR as '||'",'||',"1=0 || 1=1",$logical_value,0],
   ["PASSWORD","password","password('hello')",0,2],
   ["PASTE", "paste", "paste('ABCDEFG',3,2,'1234')","AB1234EFG",1],
   ["PATINDEX","patindex","patindex('%a%','crash')",3,0],
   ["PERIOD_ADD","period_add","period_add(9602,-12)",199502,0],
   ["PERIOD_DIFF","period_diff","period_diff(199505,199404)",13,0],
   ["POW","pow","pow(3,2)",9,0],
   ["RANGE","range","range(a)","0.0",0], # informix range(a) = max(a) - min(a)
   ["REGEXP in SELECT","regexp","'a' regexp '^(a|b)*\$'",$logical_value,0],
   ["REPLICATE","replicate","replicate('a',5)","aaaaa",1],
   ["REVERSE","reverse","reverse('abcd')","dcba",1],
   ["ROOT","root","root(4)",2,0], # informix
   ["ROUND(1 arg)","round1","round(5.63)","6",0],
   ["RPAD","rpad","rpad('hi',4,'??')",'hi??',3],
   ["SEC_TO_TIME","sec_to_time","sec_to_time(5001)","01:23:21",1],
   ["SINH","sinh","sinh(1)","1.17520119",0], # oracle hyperbolic sine of n
   ["STR","str","str(123.45,5,1)",123.5,3],
   ["STRCMP","strcmp","strcmp('abc','adc')",-1,0],
   ["STUFF","stuff","stuff('abc',2,3,'xyz')",'axyz',3],
   ["SUBSTRB", "substrb", "SUBSTRB('ABCDEFG',5,4.2)"  ,"CD",1], # oracle substring with bytes
   ["SUBSTRING as MID","mid","mid('hello',3,2)","ll",1],
   ["SUBSTRING_INDEX","substring_index","substring_index('www.tcx.se','.',-2)", "tcx.se",1],
   ["SYSDATE","sysdate","sysdate()",0,2],
   ["TAIL","tail","tail('ABCDEFG',3)","EFG",0],
   ["TANH","tanh","tanh(1)","0.462117157",0], # oracle hyperbolic tangent of n
   ["TIME_TO_SEC","time_to_sec","time_to_sec('01:23:21')","5001",0],
   ["TO_DAYS","to_days","to_days(DATE '1996-01-01')",729024,0],
   ["TRANSLATE","translate","translate('abc','bc','de')",'ade',3],
   ["TRIM; Many char extension","trim_many_char","trim(':!' FROM ':abc!')","abc",3],
   ["TRIM; Substring extension","trim_substring","trim('cb' FROM 'abccb')","abc",3],
   ["TRUNC","trunc","trunc(18.18,-1)",10,0], # oracle
   ["UID","uid","uid",0,2], # oracle uid from user
   ["UNIX_TIMESTAMP","unix_timestamp","unix_timestamp()",0,2],
   ["USERENV","userenv","userenv",0,2], # oracle user enviroment
   ["VERSION","version","version()",0,2],
   ["WEEKDAY","weekday","weekday(DATE '1997-11-29')",5,0],
   ["automatic num->string convert","auto_num2string","concat('a',2)","a2",1],
   ["automatic string->num convert","auto_string2num","'1'+2",3,0],
   ["concatenation with +","concat_as_+","'abc' + 'def'","abcdef",1],
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
   ["COUNT(DISTINCT expr,expr,...)","count_distinct_list","count(distinct a,b)",1,0],
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
 ["MATCH UNIQUE","match_unique","1 match unique (select a from crash_me)",1,0],
 ["MATCH","match","1 match (select a from crash_me)",1,0],
 ["MATCHES","matches","b matcjhes 'a*'",1,0],
 ["NOT BETWEEN","not_between","7 not between 4 and 6",1,0],
 ["NOT EXISTS","not_exists","not exists (select * from crash_me where a = 2)",1,0],
 ["NOT LIKE","not_like","b not like 'b%'",1,0],
 ["NOT UNIQUE","not_unique","not unique (select * from crash_me where a = 2)",1,0],
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

  check_and_report("Is 1+NULL = NULL","null_num_expr",
		   [],"select 1+$numeric_null $end_query",[],undef(),4);
  $tmp=sql_concat("'a'",$char_null);
  if (defined($tmp))
  {
    check_and_report("Is $tmp = NULL", "null_concat_expr", [],
		     "select $tmp $end_query",[], undef(),4);
  }
  $prompt="Need to cast NULL for arithmetic";
  save_config_data("Need_cast_for_null",
		   ($numeric_null eq "NULL") ? "no" : "yes",
		   $prompt);
}
else
{
  print "\n";
}


report("LIKE on numbers","like_with_number",
       "create table crash_q (a int,b int)",
       "insert into crash_q values(10,10)",
       "select * from crash_q where a like '10'",
       "drop table crash_q");

report("column LIKE column","like_with_column",
       "create table crash_q (a char(10),b char(10))",
       "insert into crash_q values('abc','abc')",
       "select * from crash_q where a like b",
       "drop table crash_q");

report("update of column= -column","NEG",
       "create table crash_q (a integer)",
       "insert into crash_q values(10)",
       "update crash_q set a=-a",
       "drop table crash_q");

if ($limits{'func_odbc_left'} eq 'yes' ||
    $limits{'func_odbc_substring'} eq 'yes')
{
  my $type= ($limits{'func_odbc_left'} eq 'yes' ?
	     "left(a,4)" : "substring(a for 4)");

    check_and_report("String functions on date columns","date_as_string",
		     ["create table crash_me2 (a date not null)",
		      "insert into crash_me2 values ('1998-03-03')"],
		     "select $type from crash_me2",
		     ["drop table crash_me2"],
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
		   ["drop table crash_q"],
		   "a",1,undef(),2))
  {
    check_and_report("Update with many tables","multi_table_update",
		     ["create table crash_q (a integer,b char(10))",
		      "insert into crash_q values(1,'c')",
		      "update crash_q,crash_me set crash_q.b=crash_me.b where crash_q.a=crash_me.a"],
		     "select b from crash_q",
		     ["drop table crash_q"],
		     "a",1,
		    1);
  }
}

report("DELETE FROM table1,table2...","multi_table_delete",
       "create table crash_q (a integer,b char(10))",
       "insert into crash_q values(1,'c')",
       "delete crash_q.* from crash_q,crash_me where crash_q.a=crash_me.a",
       "drop table crash_q");

check_and_report("Update with sub select","select_table_update",
		 ["create table crash_q (a integer,b char(10))",
		  "insert into crash_q values(1,'c')",
		  "update crash_q set b= (select b from crash_me where crash_q.a = crash_me.a)"],
		 "select b from crash_q",
		 ["drop table crash_q"],
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
			    ["drop table crash_q"],
			    min($max_string_size,$limits{'query_size'}-30)));

}

# It doesn't make lots of sense to check for string lengths much bigger than
# what can be stored...

find_limit(($prompt="constant string size in where"),"where_string_size",
	   new query_repeat([],"select a from crash_me where b <'",
			    "","","a","","'"));
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
       "drop table crash_q");

report("unique in create table",'unique_in_create',
       "create table crash_q (q integer not null,unique (q))",
       "drop table crash_q");

if ($limits{'unique_in_create'} eq 'yes')
{
  report("unique null in create",'unique_null_in_create',
	 "create table crash_q (q integer,unique (q))",
	 "insert into crash_q (q) values (NULL)",
	 "insert into crash_q (q) values (NULL)",
	 "insert into crash_q (q) values (1)",
	 "drop table crash_q");
}

report("default value for column",'create_default',
       "create table crash_q (q integer default 10 not null)",
       "drop table crash_q");

report("default value function for column",'create_default_func',
       "create table crash_q (q integer not null,q1 integer default (1+1)",
       "drop table crash_q");

report("temporary tables",'tempoary_table',
       "create temporary table crash_q (q integer not null)",
       "drop table crash_q");

report("create table from select",'create_table_select',
       "create table crash_q SELECT * from crash_me",
       "drop table crash_q");

report("index in create table",'index_in_create',
       "create table crash_q (q integer not null,index (q))",
       "drop table crash_q");

# The following must be executed as we need the value of end_drop_keyword
# later
if (! defined($limits{'create_index'}) &&
	! defined($limits{'drop_index'}) )
{
  if ($res=safe_query("create index crash_q on crash_me (a)"))
  {
    $res="yes";
    $drop_res="yes";
    $end_drop_keyword="";
    if (!safe_query("drop index crash_q"))
    {
      # Can't drop the standard way; Check if mSQL
      if (safe_query("drop index crash_q from crash_me"))
      {
        $drop_res="with 'FROM'";	# Drop is not ANSI SQL
        $end_drop_keyword="drop index %i from %t";
      }
      # else check if Access or MySQL
      elsif (safe_query("drop index crash_q on crash_me"))
      {
        $drop_res="with 'ON'";	# Drop is not ANSI SQL
        $end_drop_keyword="drop index %i on %t";
      }
      # else check if MS-SQL
      elsif (safe_query("drop index crash_me.crash_q"))
      {
        $drop_res="with 'table.index'"; # Drop is not ANSI SQL
        $end_drop_keyword="drop index %t.%i";
      }
    }
    else
    {
      # Old MySQL 3.21 supports only the create index syntax
      # This means that the second create doesn't give an error.
      $res=safe_query(["create index crash_q on crash_me (a)",
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
		 ["drop table crash_q"],
		 undef(),4);

if ($limits{'unique_in_create'} eq 'yes')
{
  report("null in unique index",'null_in_unique',
          create_table("crash_q",["q integer"],["unique(q)"]),
	 "insert into crash_q (q) values(NULL)",
	 "insert into crash_q (q) values(NULL)",
	 "drop table crash_q");
}

if ($limits{'null_in_unique'} eq 'yes')
{
  report("null in unique index",'multi_null_in_unique',
          create_table("crash_q",["q integer, x integer"],["unique(q)"]),
	 "insert into crash_q(x) values(1)",
	 "insert into crash_q(x) values(2)",
	 "drop table crash_q");
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
	    "drop table CRASH_Q"))
{
  safe_query("drop table crash_q");
}

if (!report("drop table if exists","drop_if_exists",
	    "create table crash_q (q integer)",
	    "drop table if exists crash_q"))
{
  safe_query("drop table crash_q");
}

report("create table if not exists","create_if_not_exists",
       "create table crash_q (q integer)",
       "create table if not exists crash_q (q integer)");
safe_query("drop table crash_q");

#
# test of different join types
#

assert("create table crash_me2 (a integer not null,b char(10) not null, c integer)");
assert("insert into crash_me2 (a,b,c) values (1,'b',1)");
assert("create table crash_me3 (a integer not null,b char(10) not null)");
assert("insert into crash_me3 (a,b) values (1,'b')");

report("inner join","inner_join",
       "select crash_me.a from crash_me inner join crash_me2 ON crash_me.a=crash_me2.a");
report("left outer join","left_outer_join",
       "select crash_me.a from crash_me left join crash_me2 ON crash_me.a=crash_me2.a");
report("natural left outer join","natural_left_outer_join",
       "select c from crash_me natural left join crash_me2");
report("left outer join using","left_outer_join_using",
       "select c from crash_me left join crash_me2 using (a)");
report("left outer join odbc style","odbc_left_outer_join",
       "select crash_me.a from { oj crash_me left outer join crash_me2 ON crash_me.a=crash_me2.a }");
report("right outer join","right_outer_join",
       "select crash_me.a from crash_me right join crash_me2 ON crash_me.a=crash_me2.a");
report("full outer join","full_outer_join",
       "select crash_me.a from crash_me full join crash_me2 ON crash_me.a=crash_me2.a");
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
       "select c from crash_me natural join crash_me2");
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

assert("drop table crash_me2");
assert("drop table crash_me3");

# somethings to be added here ....
# FOR UNION - INTERSECT - EXCEPT -> CORRESPONDING [ BY ]
# after subqueries:
# >ALL | ANY | SOME - EXISTS - UNIQUE

if (report("subqueries","subqueries",
	   "select a from crash_me where crash_me.a in (select max(a) from crash_me)"))
{
    $tmp=new query_repeat([],"select a from crash_me","","",
			  " where a in (select a from crash_me",")",
			  "",[],$max_join_tables);
    find_limit("recursive subqueries", "recursive_subqueries",$tmp);
}

report("insert INTO ... SELECT ...","insert_select",
       "create table crash_q (a int)",
       "insert into crash_q (a) SELECT crash_me.a from crash_me",
       "drop table crash_q");

report_trans("transactions","transactions",
	     [create_table("crash_q",["a integer not null"],[]),
	      "insert into crash_q values (1)"],
	     "select * from crash_q",
	     "drop table crash_q"
	    );

report("atomic updates","atomic_updates",
       create_table("crash_q",["a integer not null"],["primary key (a)"]),
       "insert into crash_q values (2)",
       "insert into crash_q values (3)",
       "insert into crash_q values (1)",
       "update crash_q set a=a+1",
       "drop table crash_q");

if ($limits{'atomic_updates'} eq 'yes')
{
  report_fail("atomic_updates_with_rollback","atomic_updates_with_rollback",
	      create_table("crash_q",["a integer not null"],
			   ["primary key (a)"]),
	      "insert into crash_q values (2)",
	      "insert into crash_q values (3)",
	      "insert into crash_q values (1)",
	      "update crash_q set a=a+1 where a < 3",
	      "drop table crash_q");
}

# To add with the views:
# DROP VIEW - CREAT VIEW *** [ WITH [ CASCADE | LOCAL ] CHECK OPTION ]
report("views","views",
       "create view crash_q as select a from crash_me",
       "drop view crash_q");

report("foreign key syntax","foreign_key_syntax",
       create_table("crash_q",["a integer not null"],["primary key (a)"]),
       create_table("crash_q2",["a integer not null",
				"foreign key (a) references crash_q (a)"],
		    []),
       "insert into crash_q values (1)",
       "insert into crash_q2 values (1)",
       "drop table crash_q2",
       "drop table crash_q");

if ($limits{'foreign_key_syntax'} eq 'yes')
{
  report_fail("foreign keys","foreign_key",
	      create_table("crash_q",["a integer not null"],
			   ["primary key (a)"]),
	      create_table("crash_q2",["a integer not null",
				       "foreign key (a) references crash_q (a)"],
			   []),
	      "insert into crash_q values (1)",
	      "insert into crash_q2 values (2)",
	      "drop table crash_q2",
	      "drop table crash_q");
}

report("Create SCHEMA","create_schema",
       "create schema crash_schema create table crash_q (a int) create table crash_q2(b int)",
       "drop schema crash_schema cascade");

if ($limits{'foreign_key'} eq 'yes')
{
  if ($limits{'create_schema'} eq 'yes')
  {
    report("Circular foreign keys","foreign_key_circular",
           "create schema crash_schema create table crash_q (a int primary key, b int, foreign key (b) references crash_q2(a)) create table crash_q2(a int, b int, primary key(a), foreign key (b) references crash_q(a))",
           "drop schema crash_schema cascade");
  }
}

report("Column constraints","constraint_check",
       "create table crash_q (a int check (a>0))",
       "drop table crash_q");

report("Table constraints","constraint_check_table",
       "create table crash_q (a int ,b int, check (a>b))",
       "drop table crash_q");

report("Named constraints","constraint_check",
       "create table crash_q (a int ,b int, constraint abc check (a>b))",
       "drop table crash_q");

report("NULL constraint (SyBase style)","constraint_null",
       "create table crash_q (a int null)",
       "drop table crash_q");

report("Triggers (ANSI SQL)","psm_trigger",
       "create table crash_q (a int ,b int)",
       "create trigger crash_trigger after insert on crash_q referencing new table as new_a when (localtime > time '18:00:00') begin atomic end",
       "insert into crash_q values(1,2)",
       "drop trigger crash_trigger",
       "drop table crash_q");

report("PSM procedures (ANSI SQL)","psm_procedures",
       "create table crash_q (a int,b int)",
       "create procedure crash_proc(in a1 int, in b1 int) language sql modifies sql data begin declare c1 int; set c1 = a1 + b1; insert into crash_q(a,b) values (a1,c1); end",
       "call crash_proc(1,10)",
       "drop procedure crash_proc",
       "drop table crash_q");

report("PSM modules (ANSI SQL)","psm_modules",
       "create table crash_q (a int,b int)",
       "create module crash_m declare procedure crash_proc(in a1 int, in b1 int) language sql modifies sql data begin declare c1 int; set c1 = a1 + b1; insert into crash_q(a,b) values (a1,c1); end; declare procedure crash_proc2(INOUT a int, in b int) contains sql set a = b + 10; end module",
       "call crash_proc(1,10)",
       "drop module crash_m cascade",
       "drop table crash_q cascade");

report("PSM functions (ANSI SQL)","psm_functions",
       "create table crash_q (a int)",
       "create function crash_func(in a1 int, in b1 int) returns int language sql deterministic contains sql begin return a1 * b1; end",
       "insert into crash_q values(crash_func(2,4))",
       "select a,crash_func(a,2) from crash_q",
       "drop function crash_func cascade",
       "drop table crash_q");

report("Domains (ANSI SQL)","domains",
       "create domain crash_d as varchar(10) default 'Empty' check (value <> 'abcd')",
       "create table crash_q(a crash_d, b int)",
       "insert into crash_q(a,b) values('xyz',10)",
       "insert into crash_q(b) values(10)",
       "drop table crash_q",
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
	   "drop table crash_q,crash_q2"))
{
  $dbh->do("drop table crash_q");
  $dbh->do("drop table crash_q2");
}


report("-- as comment","comment_--",
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
       "drop table crash_q");

report("Having with alias","having_with_alias",
       create_table("crash_q",["a integer"],[]),
       "insert into crash_q values (10)",
       "select sum(a) as b from crash_q group by a having b > 0",
       "drop table crash_q");

#
# test name limits
#

find_limit("table name length","max_table_name",
	   new query_many(["create table crash_q%s (q integer)",
			   "insert into crash_q%s values(1)"],
			   "select * from crash_q%s",1,
			   ["drop table crash_q%s"],
			   $max_name_length,7,1));

find_limit("column name length","max_column_name",
	   new query_many(["create table crash_q (q%s integer)",
			  "insert into crash_q (q%s) values(1)"],
			  "select q%s from crash_q",1,
			  ["drop table crash_q"],
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
			  ["drop table crash_q"],
			  min($max_string_size,$limits{'query_size'})));

if ($limits{'type_sql_varchar(1_arg)'} eq 'yes')
{
  find_limit("max varchar() size","max_varchar_size",
	     new query_many(["create table crash_q (q varchar(%d))",
			     "insert into crash_q values ('%s')"],
			    "select * from crash_q","%s",
			    ["drop table crash_q"],
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
			    ["drop table crash_q"],
			    min($max_string_size,$limits{'query_size'}-30)));

}

$tmp=new query_repeat([],"create table crash_q (a integer","","",
		      ",a%d integer","",")",["drop table crash_q"],
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
			     "drop table crash_q",
			     $max_keys,0));

  find_limit("index parts","max_index_parts",
	     new query_table("create table crash_q ($key_definitions,unique (q0",
			     ",q%d","))",
			     ["insert into crash_q ($key_fields) values ($key_values)"],
			     "select q0 from crash_q",1,
			     "drop table crash_q",
			     $max_keys,1));

  find_limit("max index part length","max_index_part_length",
	     new query_many(["create table crash_q (q char(%d) not null,unique(q))",
			     "insert into crash_q (q) values ('%s')"],
			    "select q from crash_q","%s",
			    ["drop table crash_q"],
			    $limits{'max_char_size'},0));

  if ($limits{'type_sql_varchar(1_arg)'} eq 'yes')
  {
    find_limit("index varchar part length","max_index_varchar_part_length",
	     new query_many(["create table crash_q (q varchar(%d) not null,unique(q))",
			     "insert into crash_q (q) values ('%s')"],
			    "select q from crash_q","%s",
			    ["drop table crash_q"],
			    $limits{'max_varchar_size'},0));
  }
}


if ($limits{'create_index'} ne 'no')
{
  if ($limits{'create_index'} eq 'ignored' ||
      $limits{'unique_in_create'} eq 'yes')
  {                                     # This should be true
    save_config_data('max_index',$limits{'max_unique_index'},"max index");
    print "indexes: $limits{'max_index'}\n";
  }
  else
  {
    if (!defined($limits{'max_index'}))
    {
      assert("create table crash_q ($key_definitions)");
      for ($i=1; $i <= min($limits{'max_columns'},$max_keys) ; $i++)
      {
	last if (!safe_query("create index crash_q$i on crash_q (q$i)"));
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
      assert("drop table crash_q");
    }
    print "indexs: $limits{'max_index'}\n";
    if (!defined($limits{'max_unique_index'}))
    {
      assert("create table crash_q ($key_definitions)");
      for ($i=0; $i < min($limits{'max_columns'},$max_keys) ; $i++)
      {
	last if (!safe_query("create unique index crash_q$i on crash_q (q$i)"));
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
      assert("drop table crash_q");
    }
    print "unique indexes: $limits{'max_unique_index'}\n";
    if (!defined($limits{'max_index_parts'}))
    {
      assert("create table crash_q ($key_definitions)");
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
      assert("drop table crash_q");
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
			       "drop table crash_q"],
			      min($limits{'max_char_size'},"+8192")));
  }
}

find_limit("index length","max_index_length",
	   new query_index_length("create table crash_q ",
				  "drop table crash_q",
				  $max_key_length));

find_limit("max table row length (without blobs)","max_row_length",
	   new query_row_length("crash_q ",
				"not null",
				"drop table crash_q",
				min($max_row_length,
				    $limits{'max_columns'}*
				    min($limits{'max_char_size'},255))));

find_limit("table row length with nulls (without blobs)",
	   "max_row_length_with_null",
	   new query_row_length("crash_q ",
				"",
				"drop table crash_q",
				$limits{'max_row_length'}*2));

find_limit("number of columns in order by","columns_in_order_by",
	   new query_many(["create table crash_q (%F)",
			   "insert into crash_q values(%v)",
			   "insert into crash_q values(%v)"],
			  "select * from crash_q order by %f",
			  undef(),
			  ["drop table crash_q"],
			  $max_order_by));

find_limit("number of columns in group by","columns_in_group_by",
	   new query_many(["create table crash_q (%F)",
			   "insert into crash_q values(%v)",
			   "insert into crash_q values(%v)"],
			  "select %f from crash_q group by %f",
			  undef(),
			  ["drop table crash_q"],
			  $max_order_by));

#
# End of test
#

$dbh->do("drop table crash_me");        # Remove temporary table

print "crash-me safe: $limits{'crash_me_safe'}\n";
print "reconnected $reconnect_count times\n";

$dbh->disconnect || warn $dbh->errstr;
save_all_config_data();
exit 0;

sub usage
{
    print <<EOF;
$0  Ver $version

This program tries to find all limits and capabilities for a SQL
server.  As it will use the server in some 'unexpected' ways, one
shouldn\'t have anything important running on it at the same time this
program runs!  There is a slight chance that something unexpected may
happen....

As all used queries are legal according to some SQL standard. any
reasonable SQL server should be able to run this test without any
problems.

All questions is cached in $opt_dir/'server_name'.cfg that future runs will use
limits found in previous runs. Remove this file if you want to find the
current limits for your version of the database server.

This program uses some table names while testing things. If you have any
tables with the name of 'crash_me' or 'crash_qxxxx' where 'x' is a number,
they will be deleted by this test!

$0 takes the following options:

--help or --Information
  Shows this help

--batch-mode
  Don\'t ask any questions, quit on errors.

--comment='some comment'
  Add this comment to the crash-me limit file

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
  Known servers names are: Access, Adabas, AdabasD, Empress, Oracle, Informix, DB2, Mimer, mSQL, MS-SQL, MySQL, Pg, Solid or Sybase.
  For others $0 can\'t report the server version.

--user='user_name'
  User name to log into the SQL server.

--start-cmd='command to restart server'
  Automaticly restarts server with this command if the server dies.

--sleep='time in seconds' (Default $opt_sleep)
  Wait this long before restarting server.

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
memory.  Your tests WILL adversely affect system performance. It's
not uncommon that either this crash-me test program, or the actual
database back-end, will DIE with an out-of-memory error. So might
any other program on your system if it requests more memory at the
wrong time.

Note also that while crash-me tries to find limits for the database server
it will make a lot of queries that can't be categorized as 'normal'.  It's
not unlikely that crash-me finds some limit bug in your server so if you
run this test you have to be prepared that your server may die during it!

We, the creators of this utility, are not responsible in any way if your
database server unexpectedly crashes while this program tries to find the
limitations of your server. By accepting the following question with 'yes',
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
  $name= `uname -s -r -m`;
  if ($?)
  {
    $name= `uname -s -m`;
  }
  if ($?)
  {
    $name= `uname -s`;
  }
  if ($?)
  {
    $name= `uname`;
  }
  if ($?)
  {
    $name="unknown";
  }
  chomp($name); $name =~ s/[\n\r]//g;
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
    print "Error: $DBI::errstr;  $server->{'data_source'}  - '$opt_user' - '$opt_password'\n";
    print "I got the above error when connecting to $opt_server\n";
    if (defined($object) && defined($object->{'limit'}))
    {
      print "This check was done with limit: $object->{'limit'}.\nNext check will be done with a smaller limit!\n";
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
# Check if the server is upp and running. If not, ask the user to restart it
#

sub check_connect
{
  my ($object)=@_;
  my ($sth);
  print "Checking connection\n" if ($opt_log_all_queries);
  # The following line will not work properly with interbase
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
# Note that all rows are executed (to ensure that we execute drop table commands)
#

sub safe_query
{
  my($queries)=@_;
  my($query,$ok,$retry_ok,$retry,@tmp,$sth);
  $ok=1;
  if (ref($queries) ne "ARRAY")
  {
    push(@tmp,$queries);
    $queries= \@tmp;
  }
  foreach $query (@$queries)
  {
    printf "query1: %-80.80s ...(%d - %d)\n",$query,length($query),$retry_limit  if ($opt_log_all_queries);
    print LOG "$query;\n" if ($opt_log);
    if (length($query) > $query_size)
    {
      $ok=0;
      next;
    }

    $retry_ok=0;
    for ($retry=0; $retry < $retry_limit ; $retry++)
    {
      if (! ($sth=$dbh->prepare($query)))
      {
	print_query($query);
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
    save_config_data($limit,safe_query(\@queries) ? "yes" : "no",$prompt);
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
    save_config_data($limit,safe_query(\@queries) ? "no" : "yes",$prompt);
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
    $result="no";
    foreach $query (@$queries)
    {
      if (safe_query($query->[0]))
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
    $error=safe_query_result($query,"1",2);
    save_config_data($limit,$error ? "not supported" : $last_result,$prompt);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
}

sub report_trans
{
  my ($prompt,$limit,$queries,$check,$clear)=@_;
  print "$prompt: ";
  if (!defined($limits{$limit}))
  {
    eval {undef($dbh->{AutoCommit})};
    if (!$@)
    {
      if (safe_query(\@$queries))
      {
	  $rc = $dbh->rollback;
	  if ($rc) {
	    $dbh->{AutoCommit} = 1;
	    if (safe_query_result($check,"","")) {
	      save_config_data($limit,"yes",$prompt);
	    }
	    safe_query($clear);
	  } else {
	    $dbh->{AutoCommit} = 1;
	    safe_query($clear);
	    save_config_data($limit,"error",$prompt);
	  }
      } else {
        save_config_data($limit,"error",$prompt);
      }
      $dbh->{AutoCommit} = 1;
    }
    else
    {
      save_config_data($limit,"no",$prompt);
    }
    safe_query($clear);
  }
  print "$limits{$limit}\n";
  return $limits{$limit} ne "no";
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
    $tmp=1-safe_query(\@$pre);
    $tmp=safe_query_result($query,$answer,$string_type) if (!$tmp);
    safe_query(\@$post);
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
    $type="no";			# Not supported
    foreach $test (@tests)
    {
      my $tmp_type= shift(@$test);
      if (safe_query(\@$test))
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
  my ($pre,$query,$post,$answer,$string_type)=@_;
  my ($tmp);

  $tmp=safe_query(\@$pre);
  $tmp=safe_query_result($query,$answer,$string_type) == 0 if ($tmp);
  safe_query(\@$post);
  return $tmp;
}


# returns 0 if ok, 1 if error, -1 if wrong answer
# Sets $last_result to value of query

sub safe_query_result
{
  my ($query,$answer,$result_type)=@_;
  my ($sth,$row,$result,$retry);
  undef($last_result);

  printf "\nquery3: %-80.80s\n",$query  if ($opt_log_all_queries);
  print LOG "$query;\n" if ($opt_log);
  for ($retry=0; $retry < $retry_limit ; $retry++)
  {
    if (!($sth=$dbh->prepare($query)))
    {
      print_query($query);
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
    $sth->finish;
    return ($result_type == 8) ? 0 : 1;
  }
  if(result_type == 8) {
    $sth->finish;
    return 1;
  }
  $result=0;                  	# Ok
  $last_result= $row->[0];	# Save for report_result;
  if ($result_type == 0)	# Compare numbers
  {
    $row->[0] =~ s/,/,/;	# Fix if ',' is used instead of '.'
    if ($row->[0] != $answer && (abs($row->[0]- $answer)/
				 (abs($row->[0]) + abs($answer))) > 0.01)
    {
      $result=-1;
    }
  }
  elsif ($result_type == 1)	# Compare where end space may differ
  {
    $row->[0] =~ s/\s+$//;
    $result=-1 if ($row->[0] ne $answer);
  }
  elsif ($result_type == 3)	# This should be a exact match
  {
    $result= -1 if ($row->[0] ne $answer);
  }
  elsif ($result_type == 4)	# If results should be NULL
  {
    $result= -1 if (defined($row->[0]));
  }
  elsif ($result_type == 5)	# Result should have given prefix
  {
    $result= -1 if (length($row->[0]) < length($answer) &&
		    substring($row->[0],1,length($answer)) ne $answer);
  }
  elsif ($result_type == 6)	# Exact match but ignore errors
  {
    $result= 1 if ($row->[0] ne $answer);
  }
  elsif ($result_type == 7)	# Compare against array of numbers
  {
    if ($row->[0] != $answer->[0])
    {
      $result= -1;
    }
    else
    {
      my ($value);
      shift @$answer;
      while (($row=$sth->fetchrow_arrayref))
      {
	$value=shift(@$answer);
	if (!defined($value))
	{
	  print "\nquery: $query returned to many results\n"
	    if ($opt_debug);
	  $result= 1;
	  last;
	}
	if ($row->[0] != $value)
	{
	  $result= -1;
	  last;
	}
      }
      if ($#$answer != -1)
      {
	print "\nquery: $query returned too few results\n"
	  if ($opt_debug);
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
# will prefere lower limits to get the server to crash as few times as possible
#

sub find_limit()
{
  my ($prompt,$limit,$query)=@_;
  my ($first,$end,$i,$tmp);
  print "$prompt: ";
  if (defined($end=$limits{$limit}))
  {
    print "$end (cache)\n";
    return $end;
  }
  if (defined($query->{'init'}) && !defined($end=$limits{'restart'}{'tohigh'}))
  {
    if (!safe_query($query->{'init'}))
    {
      $query->cleanup();
      return "error";
    }
  }

  if (!limit_query($query,1))           # This must work
  {
    print "\nMaybe fatal error: Can't check '$prompt' for limit=1\nerror: $last_error\n";
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

  unless(limit_query($query,0+$end)) {
    while ($first < $end)
    {
      print "." if ($opt_debug);
      save_config_data("restart",$i,"") if ($opt_restart);
      if (limit_query($query,$i))
      {
        $first=$i;
        $i=$first+int(($end-$first+1)/2); # to be a bit faster to go up
      }
      else
      {
        $end=$i-1;
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
	  $limits{$key}=$limit;
	  $prompts{$key}=length($prompt) ? substr($prompt,2) : "";
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
    die "Undefined limit for $key\n";
  }
  print CONFIG_FILE "$key=$limit\t# $prompt\n";
  $limits{$key}=$limit;
  $limit_changed=1;
  if (($opt_restart && $limits{'operating_system'} =~ /windows/i) ||
		       ($limits{'operating_system'} =~ /NT/))
  {
    # If perl crashes in windows, everything is lost (Wonder why? :)
    close CONFIG_FILE;
    open(CONFIG_FILE,"+>>$opt_config_file") ||
      die "Can't reopen configure file $opt_config_file: $!\n";
  }
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

  print CONFIG_FILE "#This file is automaticly generated by crash-me $version\n\n";
  foreach $key (sort keys %limits)
  {
    $tmp="$key=$limits{$key}";
    print CONFIG_FILE $tmp . ("\t" x (int((32-min(length($tmp),32)+7)/8)+1)) .
      "# $prompts{$key}\n";
  }
  close CONFIG_FILE;
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
  my($table_name,$fields,$index) = @_;
  my($query,$nr,$parts,@queries,@index);

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
  $query.= ')';
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
  $size=main::min($main::limits{'max_index_part_length'},$main::limits{'max_char_size'});
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
