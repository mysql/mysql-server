#!/usr/bin/perl
#
# Tests MySQL. Output is given to the stderr. Use
# diff to check the possible differencies.
#

use DBI;
use Getopt::Long;

$VER = "1.9";
$| = 1;

$opt_db =            "test";
$opt_user =         $opt_password = $opt_without = "";
$opt_host =         "localhost";
$opt_port =          "3306";
$opt_socket =      "/tmp/mysql.sock";
$opt_help =          0;

$NO_ERR  = 0;   # No error
$EXP_ERR = 1;   # Expect error
$MAY_ERR = 2;   # Maybe error
$HS      = 0;   # Horizontal style of output
$VS      = 1;   # Vertical style of output
$VERBOSE = 0;   # Print the results
$SILENT  = 1;   # No output

@test_packages = ("FUNC", "PROC", "SHOW");

####
#### main program
####

main();

sub main()
{
  GetOptions("help", "db=s", "port=i", "host=s", "password=s", "user=s", "socket=s", 
	      "without=s") || usage();

  usage() if ($opt_help);

  $dbh = DBI->connect("DBI:mysql:$opt_db:$opt_host:port=$opt_port:mysql_socket=$opt_socket", $opt_user, $opt_password, { PrintError => 0 }) 
  || die $DBI::errstr;

## QQ ######################################

$sth = $dbh->prepare("show felds from t2") 
|| die "Couldn't prepare query: $DBI::errstr\n";
if (!$sth->execute)
{
  print "Couldn't execute query: $DBI::errstr\n";
  $sth->finish;
  die;
}
while (($row = $sth->fetchrow_arrayref))
{
  print "$row->[1]\n";
}


exit(0);

## QQ ######################################

  printf("####\n#### THIS IS mysql-test script RUNNING\n");
  printf("####      mysql-test version $VER\n####\n");

  test_mysql_functions() if (&chk_package($opt_without, $test_packages[0]));
  test_mysql_procedures() if (&chk_package($opt_without, $test_packages[1]));
  test_mysql_show() if (&chk_package($opt_without, $test_packages[2]));

  print "\n";
  return;
}

####
#### test show -command of MySQL
####

sub test_mysql_show
{
  my ($query, $i);
  
  $query = create_show_tables();
  &exec_query(["drop table my_t"], $MAY_ERR, $SILENT);
  for ($i = 0; $query[$i]; $i++)
  {
    &exec_query([$query[$i]], $NO_ERR, $VERBOSE, $HS);
    &exec_query(["show fields from my_t"], $NO_ERR, $VERBOSE, $HS);
    &exec_query(["show keys from my_t"], $NO_ERR, $VERBOSE, $HS);
    &exec_query(["drop table my_t"], $NO_ERR, $SILENT);
  }
}

sub create_show_tables
{
  my ($query, $i);
  
  $query[0] = <<EOF;
create table my_t (i int, f float, s char(64), b blob, t text)
EOF
  $query[1] = <<EOF;
create table my_t (i int, f float, s char(64), b blob, t text, primary key (i))
EOF
  $query[2] = <<EOF;
create table my_t (i int, f float, s char(64), b blob, t text, unique (i), unique(s))
EOF
  for ($i = 0; $query[$i]; $i++) { chop($query[$i]); }
  return $query;
}

####
#### test procedures, currently only procedure analyze()
####

sub test_mysql_procedures
{
  test_analyze();
}

sub test_analyze
{
  my ($query, $i, $j);
  
  if ($opt_help)
  {
    usage();
  }
  # invalid queries
  &exec_query(["select * from mails procedure analyse(-1)"], 
	      $EXP_ERR, $VERBOSE, $HS);
  &exec_query(["select * from mails procedure analyse(10, -1)"], 
	      $EXP_ERR, $VERBOSE, $HS);
  &exec_query(["select * from mails procedure analyse(1, 2, 3)"],
	      $EXP_ERR, $VERBOSE, $HS);
  &exec_query(["select * from mails procedure analyse(-10, 10)"],
	      $EXP_ERR, $VERBOSE, $HS);
  &exec_query(["select * from mails procedure analyse('a', 'a')"],
	      $EXP_ERR, $VERBOSE, $HS);
  # valid queries
#  &exec_query(["select * from mails procedure analyse(10)"], 0, 0);
#  &exec_query(["select * from mails procedure analyse(10, 10)"], 0, 0);
#  &exec_query(["select hash from mails procedure analyse()"], 0, 0);
  &exec_query(["use mysql_test"], $NO_ERR, $VERBOSE, $HS);
#  &exec_query(["select timestamp from w32_user procedure analyse(0)"], 0, 0); 
  $query = create_test_tables();
  &exec_query(["drop table my_t"], $MAY_ERR, $SILENT);
  for ($i = 0; $query[$i][0]; $i++)
  {
    &exec_query([$query[$i][0]], $NO_ERR, $SILENT); # create table
    for ($j = 1; $query[$i][$j]; $j++)
    {
      &exec_query([$query[$i][$j]], $NO_ERR, $SILENT); # do inserts
    }
    &exec_query(["select * from my_t procedure analyse(0,0)"],
		$NO_ERR, $VERBOSE, $HS);
    &exec_query(["select * from my_t procedure analyse()"],
		$NO_ERR, $VERBOSE, $HS);
    &exec_query(["drop table my_t"], $NO_ERR, $SILENT);
  }
}

####
#### if $opt is found as a part from the '--without=...' option string
#### return 0, else 1. if zero is returned, then that part of MySQL
#### won't be tested
####

sub chk_package
{
  my ($opt_str, $opt) = @_;
  
  $sub_opt_str = '';
  for ($i = 0, $ptr = substr($opt_str, $i, 1); $ptr || $ptr eq '0';
       $i++, $ptr = substr($opt_str, $i, 1))
  {
    $sub_opt_str .= $ptr;
    if ($sub_opt_str eq $opt)
    {
      $next_chr = substr($opt_str, ($i + 1), 1);
      if ($next_chr eq ',' || (!$next_chr && $next_chr ne '0'))
      {
	return 0;
      }
    }
    if ($ptr eq ',')
    {
      # next word on the opt_str
      $sub_opt_str = '';
    }
  }
  return 1;
}

####
#### Tests given function(s) with given value(s) $count rounds
#### If function doesn't have an arg, test it once and continue.
#### ulargs (number of unlimited args) is the number of arguments 
#### to be placed in place of '.' . '.' means that any number
#### of the last argument type is possible to the function.
#### If force is given, never mind about errors
#### args: $func:   list of functions to be tested
####       $value:  list of values to be used with functions
####       $count:  number of times one function should be tested
####       $ulargs: number of unlimited args to be used when possible
####       $table_info: information about the table to be used, contains:
####       table name, info about the fields in the table, for example:
####       [mysql_test1, "Zi", "Rd"], where mysql_test1 is the name of the
####       table, "Zi" tells, that the first field name is 'i' and it is
####       type 'Z' (integer), see test_mysql_functions, 'Rd' tells that
####       the second field name is 'd' and the type is 'R' (real number)
####       $force:  if given, never mind about errors
####       $mix:    if 0, use the same argument at a time in a 
####                function that has two or more same type arguments
####                if 1, use different values
####

sub test_func()
{
  my ($func, $value, $count, $ulargs, $table_info, $force, $mix) = @_;
  my ($query, $i, $j, $k, $no_arg, $row, $ulimit, $tbinfo, $tbused, $arg);
  
  if (!$func->[0][0])
  {
    printf("No function found!\n");
    if (!$force) { die; }
  }
  
  for ($i = 0; $func->[$i][0]; $i++)
  {
    $tbused = 0;
    $no_arg = 0;
    for ($j = 0; $j < $count && !$no_arg; $j++)
    {      
      if ($tbused || $no_arg) { next; }
      $query = "select $func->[$i][0](";
      #search the values for the args
      for ($k = 0; $k < length($func->[$i][1]) && !$no_arg; $k++)
      {
	if ($mix)
	{
	  $arg = $j + 1 + $k;
	}
	else
	{
	  $arg = $j + 1;
	}
	if (substr($func->[$i][1], $k, 1) eq 'E')
	{
	  $no_arg = 1;
	  next;
	}
	if ($k) { $query .= ','; }

	if (substr($func->[$i][1], $k, 1) eq 'S')
	{
	  $query .= &find_value(\@value, 'S', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'N')
	{
	  $query .= &find_value(\@value, 'N', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'Z')
	{
	  $query .= &find_value(\@value, 'Z', $arg);
	}
	elsif ((substr($func->[$i][1], $k, 1) eq 'R'))
	{
	  $query .= &find_value(\@value, 'R', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'T')
	{
	  $query .= &find_value(\@value, 'T', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'D')
	{
	  $query .= &find_value(\@value, 'D', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'B')
	{
	  $query .= &find_value(\@value, 'B', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'C')
	{
	  $query .= &find_value(\@value, 'C', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'F')
	{
	  $query .= &find_value(\@value, 'F', $arg);
	}
	elsif (substr($func->[$i][1], $k, 1) eq '.')
	{
	  chop($query);
	  for ($ulimit = 0; $ulimit < $ulargs; $ulimit++)
	  {
	    $query .= ',';
	    $query .= &find_value(\@value,
				  substr($func->[$i][1], $k - 1, 1),
				  $j + $ulimit + 2);
	  }
	}
	elsif (substr($func->[$i][1], $k, 1) eq 'A')
	{
	  for ($tbinfo = 1; substr($table_info->[$tbinfo], 0, 1) ne
	       substr($func->[$i][1], $k + 1, 1); $tbinfo++)
	  {
	    if (!defined($table_info->[$tbinfo]))
	    {
	      printf("Illegal function structure!\n");
	      printf("A table was needed, but no type specified!\n");
	      printf("Unready query was: $query\n");
	      if (!$force) { die; }
	      else { next; }
	    }
	  }
	  if ($k) { $query .= ","; }
	  $query .= substr($table_info->[$tbinfo], 1,
			   length($table_info->[$tbinfo]) - 1);
	  $k++;
	  $tbused = 1;
	}
   	else
	{
	  printf("Not a valid type: \n");
	  printf(substr($func->[$i][1], $k, 1));
	  printf("\nAttempted to be used with unready query: \n");
	  printf("$query\n");
	}
      }
      $query .= ")";
      if ($tbused)
      {
	$query .= " from ";
	$query .= $table_info->[0];
      }
      if (!($sth = $dbh->prepare($query)))
      {
	printf("Couldn't prepare: $query\n");
	if (!$force) { die; }
      }
      if (!$sth->execute)
      {
	printf("Execution failed: $DBI::errstr\n");
	printf("Attempted query was:\n$query\n");
	$sth->finish;
	if (!$force) { die; }
      }
      else 
      { 
	printf("mysql> $query;\n");
	display($sth, 1);
	printf("Query OK\n\n");
      }
    }
  }
}

####
#### mk_str returns a string where the first arg is repeated second arg times
#### if repeat is 1, return the original str
####

sub mk_str()
{
  my ($str, $repeat) = @_;
  my ($res_str);

  if ($repeat <= 0)
  {
    die "Invalid repeat times!\n";
  }
  
  for ($repeat--, $res_str = $str; $repeat > 0; $repeat--)
  {
    $res_str .= $str;
  }
  return $res_str;
}

####
#### find_value: returns a value from list of values
#### args: $values:  list of values
####       $type:    type of argument (S = string, N = integer etc.)
####       $ordinal: the ordinal number of an argument in the list
####

sub find_value()
{
  my ($values, $type, $ordinal) = @_;
  my ($total, $i, $j, $tmp, $val);

  $total = -1; # The first one is the type

  for ($i = 0; $values[$i][0]; $i++)
  {
    if ($values[$i][0] eq $type)
    {
      $tmp = $values[$i];
      foreach $val (@$tmp) { $total++; }
      for ( ;$total < $ordinal; )
      {
	$ordinal -= $total;
      }
      return $values[$i][$ordinal];
    }
  }
  printf("No type '$type' found in values\n");
  die;
}

####
#### exec_query: execute a query, print information if wanted and exit
#### args: $queries:      list of queries to be executed
####       $expect_error: if 0, error is not expected. In this case if an
####                      error occurs, inform about it and quit
####                      if 1, error is expected. In this case if sql server
####                      doesn't give an error message, inform about it
####                      and quit
####                      if 2, error may happen or not, don't care
####       $silent:       if true, reduce output
####       $style:        type of output, 0 == horizontal, 1 == vertical
####

sub exec_query()
{
  my ($queries, $expect_error, $silent, $style) = @_;
  my ($query);

  foreach $query (@$queries)
  {
    if (!($sth = $dbh->prepare($query)))
    {
      printf("Couldn't prepare: $query\n");
      die;
    }
    if (!$sth->execute)
    {
      if ($expect_error == 1)
      {
        printf("An invalid instruction was purposely made,\n"); 
        printf("server failed succesfully:\n");
        printf("$DBI::errstr\n");
        printf("Everything OK, continuing...\n");
        return;
      }
      if ($expect_error != 2)
      {
        printf("Execution failed: $DBI::errstr\n");
        printf("Attempted query was:\n$query\n");
        die;
      }
    }
    if ($expect_error == 1)
    {
      printf("An invalid instruction was purposely made,\n");
      printf("server didn't note, ALARM!\n");
      printf("The query made was: $query\n");
      printf("The output from the server:\n");
    }
    if ($expect_error == 2) { return; }
    if (!$silent) { printf("mysql> $query;\n"); }
    display($sth, $style);
    if (!$silent) { printf("Query OK\n\n"); }
    if ($expect_error) { die; }
  }
  return;
}

####
#### Display to stderr
#### Args: 1: ($sth) statememt handler
####       2: ($style) 0 == horizontal style, 1 == vertical style
####

sub display()
{
  my ($sth, $style) = @_;
  my (@data, @max_length, $row, $nr_rows, $nr_cols, $i, $j, $tmp, $mxl);
  
  # Store the field names and values in @data.
  # Store the max field lengths in @max_length
  for ($i = 0; ($row = $sth->fetchrow_arrayref); $i++)
  {
    if (!$i)
    {
      $nr_cols = $#$row;
      for ($j = 0; $j <= $#$row; $j++)
      {
        $data[$i][$j] = $sth->{NAME}->[$j];
        $max_length[$j] = length($data[$i][$j]);
      }
      $i++;
    }
    for ($j = 0; $j <= $#$row; $j++)
    {
      $data[$i][$j] = $row->[$j];
      $max_length[$j] = $tmp if ($max_length[$j] < 
				 ($tmp = length($data[$i][$j])));
    }
  }
  if (!($nr_rows = $i))
  {
    return;
  }
  # Display data
  if ($style == 0)
  {
    for ($i = 0; $i < $nr_rows; $i++)
    {
      if (!$i)
      {
        for ($j = 0; $j <= $nr_cols; $j++)
        {
          print "+"; print "-" x ($max_length[$j] + 2);
        }
        print "+\n";
      }
      print "|";
      for ($j = 0; $j <= $nr_cols; $j++)
      {
        print " ";
        if (defined($data[$i][$j]))
        {
          print $data[$i][$j];
	  $tmp = length($data[$i][$j]);
        }
	else
	{
	  print "NULL";
	  $tmp = 4;
	}
        print " " x ($max_length[$j] - $tmp);
        print " |";
      }
      print "\n";
      if (!$i)
      {
        for ($j = 0; $j <= $nr_cols; $j++)
        {
          print "+"; print "-" x ($max_length[$j] + 2);
        }
        print "+\n";
      }
    }
    for ($j = 0; $j <= $nr_cols; $j++)
    {
      print "+"; print "-" x ($max_length[$j] + 2);
    }
    print "+\n";
    return;
  }
  if ($style == 1)
  {
    for ($i = 0; $max_length[$i]; $i++)
    {
      $mxl = $max_length[$i] if ($mxl < $max_length[$i]);
    }

    for ($i = 1; $i < $nr_rows; $i++)
    {
      print "*" x 27;
      print " " . $i . ". row ";
      print "*" x 27;
      print "\n";
      for ($j = 0; $j <= $nr_cols; $j++)
      {
	print " " x ($mxl - length($data[0][$j]));
	print "$data[0][$j]: ";
	if (defined($data[$i][$j]))
	{
	  print "$data[$i][$j] \n";
	}
	else
	{
	  print "NULL\n";
	}
      }
    }
    return;
  }
}

####
#### usage
####

sub usage
{
    print <<EOF;
mysql-test $VER by Jani Tolonen

Usage: mysql-test [options]

Options:
--help      Show this help
--db=       Database to use (Default: $opt_db)
--port=     TCP/IP port to use for connection (Default: $opt_port)
--socket=   UNIX socket to use for connection (Default: $opt_socket)
--host=     Connect to host (Default: $opt_host)
--user=     User for login if not current user
--password  Password to use when connecting to server

--without=PART_NAME1,PART_NAME2,...  
            test without a certain part of MySQL, optional parts listed below

Optional parts:

FUNC        Ignore MySQL basic functions
PROC        Ignore MySQL procedure functions
EOF
  exit(0);
}


sub test_mysql_functions
{
  
  ####
  #### MySQL functions
  ####
  #### Types: S = string (or real number) , N = unsigned integer, Z = integer, 
  ####        R = real number, T = time_stamp, E = no argument, D = date, 
  ####        B = boolean, C = character
  ####        F = format (usually used with the date-types)
  ####        . = any number of the last argument type possible
  ####        A = require table for test, the following argument 
  ####            is the argument for the function
  
  # Muista get_lock,group_unique_users,
  # position, unique_users
  
  # ks. kaikki date function, kerää yhteen, testaa erikseen
  # adddate, date_add, subdate, date_sub, between, benchmark, count

  # decode, encode, get_lock, make_set, position
  
  @functions = (["abs","R"],["acos","R"],["ascii","C"],["asin","R"],
		["atan","R"],["atan2","R"],["avg","AR"],["bin","Z"],
		["bit_count","Z"],["bit_or","AZ"],["bit_and","AZ"],
		["ceiling","R"],["char","N."],["char_length","S"],
		["concat","SS."],["conv","ZZZ"],
		["cos","R"],["cot","R"],["curdate","E"],
		["curtime","E"],["database","E"],["date_format","DF"],
		["dayofmonth","D"],["dayofyear","D"],["dayname","D"],
		["degrees","R"],["elt","NS."],["encode","SS"],
		["encrypt","S"],["encrypt","SS"],["exp","R"],["field","SS."],
		["find_in_set","SS"],["floor","R"],["format","RN"],
		["from_days","N"],["from_unixtime","N"],
		["from_unixtime","NF"],["greatest","RR."],["hex","Z"],
		["hour","D"],["if","ZSS"],["ifnull","SS"],["insert","SNNS"],
		["instr","SS"],["interval","RR."],["isnull","S"],
		["last_insert_id","E"],["lcase","S"],["least","RR."],
		["left","SN"],["length","S"],["locate","SS"],
		["log","R"],["log10","R"],["lpad","SNS"],["ltrim","S"],
		["max","AR"],["mid","SNN"],["min","AR"],["minute","D"],
		["mod","ZZ"],["monthname","D"],
		["month","D"],["now","E"],["oct","Z"],
		["octet_length","S"],["password","S"],["period_add","DD"],
		["period_diff","DD"],["pi","E"],
		["pow","RR"],["quarter","D"],["radians","R"],
		["rand","E"],["rand","R"],["release_lock","S"],
		["repeat","SN"],["replace","SSS"],["reverse","S"],
		["right","SN"],["round","R"],["round","RN"],
		["rpad","SNS"],["rtrim","S"],["sec_to_time","N"],
		["second","T"],["sign","R"],["sin","R"],
		["space","N"],["soundex","S"],["sqrt","R"],["std","AR"],
		["strcmp","SS"],["substring","SN"],["substring","SNN"],
		["substring_index","SSZ"],["sum","AR"],
		["tan","R"],["time_format","TF"],["time_to_sec","T"],
		["to_days","D"],["trim","S"],
		["truncate","RN"],["ucase","S"],
		["unix_timestamp","E"],["unix_timestamp","D"],["user","E"],
		["version","E"],["week","D"],["weekday","D"],["year","D"]);

  ####
  #### Various tests for the functions above
  ####
  
  &exec_query(["drop table mysql_test1"], $MAY_ERR, $SILENT);
  
  $query .= <<EOF;
create table mysql_test1 (
  i int,
  d double
)
EOF
  chop($query);
  &exec_query([$query], $NO_ERR, $SILENT);
  
  ####
  #### Basic tests
  ####
  
  printf("####\n#### BASIC TESTS FOR FUNCTIONS\n####\n\n");

  @bunch = ("insert into mysql_test1 values(-20,-10.5),(20,10.5),(50,100.00)",
	    "insert into mysql_test1 values(100,500.333)");
  &exec_query(\@bunch, $NO_ERR, $SILENT);
  
  printf("\n####\n#### First basic test part\n####\n\n");

  @values = (["S", "'a'", "'abc'", "'abc def'", "'abcd'", "'QWERTY'", 
	      "'\\\\'", "'*.!\"#¤%&/()'", "'" . &mk_str('a',1024) . "'",
	      "?", "<>", "#__#"],
	     ["N", -1000, -500, -100, -1, 0, 1, 40, 50, 70, 90,
	      100, 500, 1000],
	     ["Z", -100, -50, 200, 1000],
	     ["R", -500.5, -10.333, 100.667, 400.0],
	     ["T", 19980728154204, 19980728154205, 19980728154206,
	      19980728154207],
	     ["D", "'1997-12-06'", "'1997-12-07'", "'1997-12-08'", 
	      "'1997-12-09'"],
	     ["B", 1, 0, 0, 1],
	     ["C", "'a'", "'e'", "'r'", "'q'"],
	     ["F", "'%a'", "'%b'", "'%d'", "'%H'"]);
  &test_func(\@functions, \@values, 4, 5, ["mysql_test1","Zi","Rd"]);

  printf("\n####\n#### Second basic test part\n####\n\n");

  @values = (["S", "'a'", "'BC'", "'def'", "'HIJK'", "'lmnop'", "'QRSTUV'"],
	     ["N", 0, 1, 2, 3, 4, 5],
	     ["Z", 0, 1, 2, 3, 4, 5],
	     ["R", 0, 1, 2, 3, 4, 5],
	     ["T", 19990608234530, 20000709014631, 20010810024732,
	      20020911034833, 20031012044934, 20041113055035],
	     ["D", "'1999-06-08'", "'2000-07-09'", "'2001-08-10'",
	      "'2002-09-11'", "'2003-10-12'", "'2004-11-13'"],
	     ["B", 0, 1, 0, 1, 0, 1],
	     ["C", "'a'", "'BC'", "'def'", "'HIJK'", "'lmnop'", "'QRSTUV'"],
	     ["F", "'%a'", "'%b'", "'%d'", "'%h'", "'%H'", "'%i'"]);
  &test_func(\@functions, \@values, 6, 6, ["mysql_test1","Zi","Rd"], 0, 1);

  printf("\n####\n#### Third basic test part\n####\n\n");

  @values = (["S", "'Monty'", "'Jani'", "'MySQL'", "''"],
	     ["N", 10, 54, -70, -499],
	     ["Z", 11.03, "'Abo'", 54.333, "''"],
	     ["R", 12, "'gnome'", -34.211, "''"],
	     ["T", 3, "'Redhat'", -19984021774433, "''"],
	     ["D", "'1990-01-31'", "'-3333-10-23'", -5631_23_12, "''"],
	     ["B", 0, "'asb'", -4, "''"],
	     ["C", "'a'", 503, -45353453, "''"],
	     ["F", "'%a'", -231, "'Mitsubishi'", "''"]);
  &test_func(\@functions, \@values, 3, 3, ["mysql_test1","Zi","Rd"], 0, 1);

  &exec_query(["delete from mysql_test1"], $NO_ERR, $SILENT);

  ####
  #### Null tests
  ####
  
  printf("\n\n####\n#### NULL TESTS FOR FUNCTIONS\n####\n\n\n");

  &exec_query(["insert into mysql_test1 values(null,null)"], $NO_ERR,
	     $SILENT);
  @values = (["S", "NULL"],
	     ["N", "NULL"],
	     ["Z", "NULL"],
	     ["R", "NULL"],
	     ["T", "NULL"],
	     ["D", "NULL"],
	     ["B", "NULL"],
	     ["C", "NULL"],
	     ["F", "NULL"]);
  &test_func(\@functions, \@values, 1, 5, ["mysql_test1","Zi","Rd"], 1);
  &exec_query(["delete from mysql_test1"], $NO_ERR, $SILENT);
  
  ####
  #### Tests to fulfill the main part of function tests above
  ####

  printf("\n\n####\n#### FULFILL TESTS \n####\n\n\n");
  
  &exec_query(["drop table my_t"], $MAY_ERR, $SILENT);
  &exec_query(["create table my_t (s1 char(64), s2 char(64))"],
	      $NO_ERR, $VERBOSE, $HS);
  $query = <<EOF;
insert into my_t values('aaa','aaa'),('aaa|qqq','qqq'),('gheis','^[^a-dXYZ]+\$'),('aab','^aa?b'),('Baaan','^Ba*n'),('aaa','qqq|aaa'),('qqq','qqq|aaa'),('bbb','qqq|aaa'),('bbb','qqq'),('aaa','aba'),(null,'abc'),('def',null),(null,null),('ghi','ghi[')
EOF
  chop($query);
  &exec_query([$query], $NO_ERR, $VERBOSE, $HS);
  &exec_query(["select s1 regexp s2 from my_t"],
	      $NO_ERR, $VERBOSE, $HS);


  ####
  #### ["position","SS"],
  ####
  
}

sub create_test_tables
{
  $query[0][0] = <<EOF;
  CREATE TABLE my_t (
  auto int(5) unsigned DEFAULT '0' NOT NULL auto_increment,
  string varchar(10) DEFAULT 'hello',
  binary_string varchar(10) binary DEFAULT '' NOT NULL,
  tiny tinyint(4) DEFAULT '0' NOT NULL,
  short smallint(6) DEFAULT '1' NOT NULL,
  medium mediumint(8) DEFAULT '0' NOT NULL,
  longint int(11) DEFAULT '0' NOT NULL,
  longlong bigint(13) DEFAULT '0' NOT NULL,
  num decimal(5,2) DEFAULT '0.00' NOT NULL,
  num_fill decimal(6,2) unsigned zerofill DEFAULT '0000.00' NOT NULL,
  real_float float(13,1) DEFAULT '0.0' NOT NULL,
  real_double double(13,1),
  utiny tinyint(3) unsigned DEFAULT '0' NOT NULL,
  ushort smallint(5) unsigned zerofill DEFAULT '00000' NOT NULL,
  umedium mediumint(8) unsigned DEFAULT '0' NOT NULL,
  ulong int(11) unsigned DEFAULT '0' NOT NULL,
  ulonglong bigint(13) unsigned DEFAULT '0' NOT NULL,
  zero int(5) unsigned zerofill,
  time_stamp timestamp(14),
  date_field date,
  time_field time,
  date_time datetime,
  blob_col blob,
  tinyblob_col tinyblob,
  mediumblob_col mediumblob NOT NULL,
  longblob_col longblob NOT NULL,
  options enum('one','two','three'),
  flags set('one','two','three'),
  PRIMARY KEY (auto)
)
EOF
  chop($query[0][0]);
  $query[0][1] = <<EOF;
  INSERT INTO my_t VALUES (1,'hello','',0,1,0,0,0,0.00,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154204,NULL,'01:00:00',
			   NULL,NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][1]);
      $query[0][2] = <<EOF;
  INSERT INTO my_t VALUES (2,'hello','',0,1,0,0,0,0.00,0000.00,
			   -340282346638528859811704183484516925440.0,NULL,0,
			   00000,0,0,0,NULL,19980728154205,NULL,NULL,NULL,NULL,
			   NULL,'','',NULL,NULL)
EOF
  chop($query[0][2]);
      $query[0][3] = <<EOF;
  INSERT INTO my_t VALUES (3,'hello','',0,1,0,0,0,0.00,0000.00,0.0,NULL,0,00000,
			   0,0,0,NULL,19980728154205,NULL,NULL,
			   '2002-12-30 22:04:02',NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][3]);
      $query[0][4] = <<EOF;
  INSERT INTO my_t VALUES (4,'hello','',0,1,0,0,0,0.00,0000.00,0.0,NULL,0,00000,
			   0,0,0,NULL,19980728154205,'1997-12-06',NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][4]);
      $query[0][5] = <<EOF;
  INSERT INTO my_t VALUES (5,'hello','',0,1,0,0,0,0.00,0000.00,0.0,NULL,0,00000,
			   0,0,0,NULL,19980728154205,NULL,'20:10:08',NULL,NULL,
			   NULL,'','',NULL,NULL)
EOF
  chop($query[0][5]);
      $query[0][6] = <<EOF;
  INSERT INTO my_t VALUES (6,'hello','',0,1,0,0,0,-0.22,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154205,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][6]);
      $query[0][7] = <<EOF;
  INSERT INTO my_t VALUES (7,'hello','',0,1,0,0,0,-0.00,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154205,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][7]);
      $query[0][8] = <<EOF;
  INSERT INTO my_t VALUES (8,'hello','',0,1,0,0,0,+0.00,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154205,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][8]);
      $query[0][9] = <<EOF;
  INSERT INTO my_t VALUES (9,'hello','',0,1,0,0,0,+0.90,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154205,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][9]);
      $query[0][10] = <<EOF;
  INSERT INTO my_t VALUES (10,'hello','',0,1,0,0,0,-999.99,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154206,NULL,NULL,NULL,NULL,
			   NULL,'','',NULL,NULL)
EOF
  chop($query[0][10]);
      $query[0][11] = <<EOF;
  INSERT INTO my_t VALUES (11,'hello','',127,32767,8388607,2147483647,
			   9223372036854775807,9999.99,9999.99,
			   329999996548271212625250308919809540096.0,9.0,255,
			   65535,16777215,4294967295,18446744073709551615,
			   4294967295,00000000000000,'9999-12-31','23:59:59',
			   '9999-12-31 23:59:59',NULL,NULL,' ',' ','',
			   'one,two,three')
EOF
  chop($query[0][11]);
      $query[0][12] = <<EOF;
  INSERT INTO my_t VALUES (12,'hello','',-128,-32768,-8388608,-2147483648,
			   -9223372036854775808,-999.99,0000.00,
			   -329999996548271212625250308919809540096.0,10.0,0,
			   00000,0,0,0,00000,00000000000000,
			   '9999-12-31','23:59:59','9999-12-31 23:59:59',NULL,
			   NULL,' ,-',' ,-','','one,two,three')
EOF
  chop($query[0][12]);
      $query[0][13] = <<EOF;
  INSERT INTO my_t VALUES (13,'hello','',0,1,0,0,0,0.09,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154223,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][13]);
      $query[0][14] = <<EOF;
  INSERT INTO my_t VALUES (14,'hello','',0,1,0,0,0,0.00,0000.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154223,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][14]);
      $query[0][15] = <<EOF;
  INSERT INTO my_t VALUES (15,'hello','',0,1,0,0,0,0.00,0044.00,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154223,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][15]);
      $query[0][16] = <<EOF;
  INSERT INTO my_t VALUES (16,'hello','',0,1,0,0,0,0.00,9999.99,0.0,NULL,0,
			   00000,0,0,0,NULL,19980728154223,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][16]);
      $query[0][17] = <<EOF;
  INSERT INTO my_t VALUES (17,'hello','',127,32767,8388607,2147483647,
			   9223372036854775807,9999.99,9999.99,
			   329999996548271212625250308919809540096.0,9.0,255,
			   65535,16777215,4294967295,18446744073709551615,
			   4294967295,00000000000000,'9999-12-31','23:59:59',
			   '9999-12-31 23:59:59',NULL,NULL,'      ',' ','',
			   'one,two,three')
EOF
  chop($query[0][17]);
      $query[0][18] = <<EOF;
  INSERT INTO my_t VALUES (18,'hello','',127,32767,8388607,2147483647,
			   9223372036854775807,9999.99,9999.99,0.0,NULL,255,
			   65535,16777215,4294967295,18446744073709551615,
			   4294967295,19980728154224,NULL,NULL,NULL,NULL,
			   NULL,'','',NULL,NULL)
EOF
  chop($query[0][18]);
      $query[0][19] = <<EOF;
  INSERT INTO my_t VALUES (19,'hello','',127,32767,8388607,2147483647,
			   9223372036854775807,9999.99,9999.99,0.0,NULL,255,
			   65535,16777215,4294967295,0,4294967295,
			   19980728154224,NULL,NULL,NULL,NULL,NULL,'','',
			   NULL,NULL)
EOF
  chop($query[0][19]);
      $query[0][20] = <<EOF;
  INSERT INTO my_t VALUES (20,'hello','',-128,-32768,-8388608,-2147483648,
			   -9223372036854775808,-999.99,0000.00,0.0,NULL,0,
			   00000,0,0,18446744073709551615,00000,19980728154224,
			   NULL,NULL,NULL,NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][20]);
      $query[0][21] = <<EOF;
  INSERT INTO my_t VALUES (21,'hello','',-128,-32768,-8388608,-2147483648,
			   -9223372036854775808,-999.99,0000.00,0.0,NULL,0,
			   00000,0,0,0,00000,19980728154225,NULL,NULL,NULL,
			   NULL,NULL,'','',NULL,NULL)
EOF
  chop($query[0][21]);
      $query[0][22] = <<EOF;
  INSERT INTO my_t VALUES (22,NULL,'1',1,1,1,1,1,1.00,0001.00,1.0,NULL,1,00001,
			   1,1,1,00001,19980728154244,NULL,NULL,NULL,NULL,NULL,
			   '1','1',NULL,NULL)
EOF
  chop($query[0][22]);
      $query[0][23] = <<EOF;
  INSERT INTO my_t VALUES (23,'2','2',2,2,2,2,2,2.00,0002.00,2.0,2.0,2,00002,
			   2,2,2,00002,00000000000000,'0000-00-00','02:00:00',
			   '0000-00-00 00:00:00','2','2','2','2','','')
EOF
  chop($query[0][23]);
      $query[0][24] = <<EOF;
  INSERT INTO my_t VALUES (24,'3','3',3,3,3,3,3,3.00,0003.00,3.0,3.0,3,00003,
			   3,3,3,00003,00000000000000,'2000-00-03','00:00:03',
			   '0000-00-00 00:00:03','3.00','3.00','3.00','3.00',
			   'three','one,two')
EOF
  chop($query[0][24]);
      $query[0][25] = <<EOF;
  INSERT INTO my_t VALUES (25,'-4.7','-4.7',-5,-5,-5,-5,-5,-4.70,0000.00,-4.7,
			   -4.7,0,00000,0,0,0,00000,00000000000000,'0000-00-00',
			   '00:00:00','0000-00-00 00:00:00','-4.70','-4.70',
			   '-4.70','-4.70','','three')
EOF
  chop($query[0][25]);
      $query[0][26] = <<EOF;
  INSERT INTO my_t VALUES (26,'+0.09','+0.09',0,0,0,0,0,+0.09,0000.00,0.1,0.1,
			   0,00000,0,0,0,00000,00000000000000,'0000-00-00',
			   '00:09:00','0000-00-00 00:00:00','+0.09','+0.09',
			   '+0.09','+0.09','','')
EOF
  chop($query[0][26]);
      $query[0][27] = <<EOF;
  INSERT INTO my_t VALUES (27,'1','1',1,1,1,1,1,1.00,0001.00,1.0,1.0,1,00001,
			   1,1,1,00001,00000000000000,'2000-00-01','00:00:01',
			   '0000-00-00 00:00:01','1','1','1','1','one','one')
EOF
  chop($query[0][27]);
      $query[0][28] = <<EOF;
  INSERT INTO my_t VALUES (28,'-1','-1',-1,-1,-1,-1,-1,-1.00,0000.00,-1.0,-1.0,
			   0,00000,0,0,18446744073709551615,00000,
			   00000000000000,'0000-00-00','00:00:00',
			   '0000-00-00 00:00:00','-1','-1','-1','-1','',
			   'one,two,three')
EOF
  chop($query[0][28]);
      $query[0][29] = <<EOF;
  INSERT INTO my_t VALUES (29,'','',0,0,0,0,0,0.00,0000.00,0.0,0.0,0,00000,0,0,
			   0,00000,00000000000000,'0000-00-00','00:00:00',
			   '0000-00-00 00:00:00','','','','','','')
EOF
  chop($query[0][29]);
  $query[1][0]  = "CREATE TABLE my_t (str char(64))";
  $query[1][1]  = "INSERT INTO my_t VALUES ('5.5')";
  $query[1][2]  = "INSERT INTO my_t VALUES ('6.8')";
  $query[2][0]  = "CREATE TABLE my_t (str char(64))";
  $query[2][1]  = <<EOF;
  INSERT INTO my_t VALUES
    ('9999999999993242342442323423443534529999.02235000054213')
EOF
  chop($query[2][1]);
  $query[3][0]  = "CREATE TABLE my_t (str char(64))";
  $query[3][1]  = <<EOF;
  INSERT INTO my_t VALUES
    ('8494357934579347593475349579347593845948793454350349543348736453')
EOF
  chop($query[3][1]);
  $query[4][0]  = "CREATE TABLE my_t (d double(20,10))";
  $query[4][1]  = "INSERT INTO my_t VALUES (10.0000000000)";
  $query[4][2]  = "INSERT INTO my_t VALUES (-10.0000000000)";
  $query[5][0]  = "CREATE TABLE my_t (d double(20,10))";
  $query[5][1]  = "INSERT INTO my_t VALUES (50000.0000000000)";
  $query[6][0]  = "CREATE TABLE my_t (d double(20,10))";
  $query[6][1]  = "INSERT INTO my_t VALUES (5000000.0000000000)";
  $query[7][0]  = "CREATE TABLE my_t (d double(20,10))";
  $query[7][1]  = "INSERT INTO my_t VALUES (500000000.0000000000)";
  $query[8][0]  = "CREATE TABLE my_t (d double(20,10))";
  $query[8][1]  = "INSERT INTO my_t VALUES (50000000000.0000000000)";
  $query[8][2]  = "INSERT INTO my_t VALUES (NULL)";
  $query[9][0]  = "CREATE TABLE my_t (d double(60,10))";
  $query[9][1]  = "INSERT INTO my_t VALUES (93850983054983462912.0000000000)";
  $query[9][2]  = "INSERT INTO my_t VALUES (93850983.3495762944)";
  $query[9][3]  = <<EOF;
  INSERT INTO my_t VALUES (938509832438723448371221493568778534912.0000000000)
EOF
  chop($query[9][3]);
  $query[10][0] = "CREATE TABLE my_t (i int(11))";
  $query[10][1] = "INSERT INTO my_t VALUES (-100)";
  $query[10][2] = "INSERT INTO my_t VALUES (-200)";
  $query[11][0] = "CREATE TABLE my_t (s char(64))";
  $query[11][1] = "INSERT INTO my_t VALUES ('100.')";
  $query[12][0] = "CREATE TABLE my_t (s char(64))";
  $query[12][1] = "INSERT INTO my_t VALUES ('1e+50')";
  $query[13][0] = "CREATE TABLE my_t (s char(64))";
  $query[13][1] = "INSERT INTO my_t VALUES ('1E+50u')";
  $query[14][0] = "CREATE TABLE my_t (s char(64))";
  $query[14][1] = "INSERT INTO my_t VALUES ('1EU50')";
  $query[15][0] = "CREATE TABLE my_t (s char(64))";
  $query[15][1] = "INSERT INTO my_t VALUES ('123.000')";
  $query[15][2] = "INSERT INTO my_t VALUES ('123.000abc')";
  $query[16][0] = "CREATE TABLE my_t (s char(128))";
  $query[16][1] = <<EOF;
  INSERT INTO my_t VALUES
  ('-999999999999999999999999999999999999999999999999999999999999999999999999')
EOF
  chop($query[16][1]);
  $query[17][0] = "CREATE TABLE my_t (s char(128))";
  $query[17][1] = "INSERT INTO my_t VALUES ('-9999999999999999')";
  $query[18][0] = "CREATE TABLE my_t (s char(128))";
  $query[18][1] = "INSERT INTO my_t VALUES ('28446744073709551615001')";
  $query[18][2] = "INSERT INTO my_t VALUES ('184467440737095516150000000')";
  $query[19][0] = "CREATE TABLE my_t (s char(128))";
  $query[19][1] = "INSERT INTO my_t VALUES ('18446744073709551615')";
  $query[20][0] = "CREATE TABLE my_t (s char(128))";
  $query[20][1] = "INSERT INTO my_t VALUES ('18446744073709551616')";
  $query[21][0] = "CREATE TABLE my_t (s char(64))";
  $query[21][1] = "INSERT INTO my_t VALUES ('00740')";
  $query[21][2] = "INSERT INTO my_t VALUES ('00740.')";
  $query[22][0] = "CREATE TABLE my_t (s char(128))";
  $query[22][1] = "INSERT INTO my_t VALUES ('-18446744073709551615')";
  $query[23][0] = "CREATE TABLE my_t (s char(32))";
  $query[23][1] = "INSERT INTO my_t VALUES ('740')";
  $query[23][2] = "INSERT INTO my_t VALUES ('12345')";
  $query[23][3] = "INSERT INTO my_t VALUES ('12345')";
  $query[24][0] = "CREATE TABLE my_t (s char(32))";
  $query[24][1] = "INSERT INTO my_t VALUES ('00740')";
  $query[24][2] = "INSERT INTO my_t VALUES ('00730')";
  $query[24][3] = "INSERT INTO my_t VALUES ('00720')";
  $query[24][4] = "INSERT INTO my_t VALUES ('12345.02')";
  $query[25][0] = "CREATE TABLE my_t (i bigint(20) unsigned)";
  $query[25][1] = "INSERT INTO my_t VALUES (3000)";
  $query[25][2] = "INSERT INTO my_t VALUES (NULL)";
  $query[25][3] = "INSERT INTO my_t VALUES (900000000003)";
  $query[25][4] = "INSERT INTO my_t VALUES (90)";
  $query[26][0] = "CREATE TABLE my_t (i int(11))";
  $query[26][1] = "INSERT INTO my_t VALUES (NULL)";
  $query[27][0] = "CREATE TABLE my_t (d date)";
  $query[27][1] = "INSERT INTO my_t VALUES ('1999-05-01')";
  $query[28][0] = "CREATE TABLE my_t (y year(4))";
  $query[28][1] = "INSERT INTO my_t VALUES (1999)";
  $query[29][0] = "CREATE TABLE my_t (s char(128))";
  $query[29][1] = "INSERT INTO my_t VALUES ('453453444451.7976')";
  $query[30][0] = "CREATE TABLE my_t (s char(128))";
  $query[30][1] = "INSERT INTO my_t VALUES('')";
  $query[31][0] = "CREATE TABLE my_t (s char(128))";
  $query[31][1] = "INSERT INTO my_t VALUES(' ')";
  return $query;
}
