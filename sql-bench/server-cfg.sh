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
#
# The configuration file for the DBI/DBD tests on different databases ....
# You will need the DBD module for the database you are running.
# Monty made this bench script and I (Luuk de Boer) rewrote it to DBI/DBD.
# Monty rewrote this again to use packages.
#
# Each database has a different package that has 3 functions:
# new		Creates a object with some standard slot
# version	Version number of the server
# create	Generates commands to create a table
#

#
# First some global functions that help use the packages:
#

sub get_server
{
  my ($name,$host,$database,$odbc,$machine,$socket,$connect_options)=@_;
  my ($server);
  if ($name =~ /mysql/i)
  { $server=new db_MySQL($host, $database, $machine, $socket,$connect_options); }
  elsif ($name =~ /pg/i)
  { $server= new db_Pg($host,$database); }
  elsif ($name =~ /msql/i)
  { $server= new db_mSQL($host,$database); }
  elsif ($name =~ /solid/i)
  { $server= new db_Solid($host,$database); }
  elsif ($name =~ /Empress/i)
  { $server= new db_Empress($host,$database); }
  elsif ($name =~ /FrontBase/i)
  { $server= new db_FrontBase($host,$database); }
  elsif ($name =~ /Oracle/i)
  { $server= new db_Oracle($host,$database); }
  elsif ($name =~ /Access/i)
  { $server= new db_access($host,$database); }
  elsif ($name =~ /Informix/i)
  { $server= new db_Informix($host,$database); }
  elsif ($name =~ /ms-sql/i)
  { $server= new db_ms_sql($host,$database); }
  elsif ($name =~ /sybase/i)
  { $server= new db_sybase($host,$database); }
  elsif ($name =~ /Adabas/i)			# Adabas has two drivers
  {
    $server= new db_Adabas($host,$database);
    if ($name =~ /AdabasD/i)
    {
      $server->{'data_source'} =~ s/:Adabas:/:AdabasD:/;
    }
  }
  elsif ($name =~ /DB2/i)
  { $server= new db_db2($host,$database); }
  elsif ($name =~ /Mimer/i)
  { $server= new db_Mimer($host,$database); }
  elsif ($name =~ /Sapdb/i)
  { $server= new db_sapdb($host,$database); }
  elsif ($name =~ /interBase/i)
  { $server= new db_interbase($host,$database); }
  else
  {
      die "Unknown sql server name used: $name\nUse one of: Access, Adabas, AdabasD, Empress, FrontBase, Oracle, Informix, InterBase, DB2, mSQL, Mimer, MS-SQL, MySQL, Pg, Solid, SAPDB or Sybase.\nIf the connection is done trough ODBC the name must end with _ODBC\n";
  }
  if ($name =~ /_ODBC$/i || defined($odbc) && $odbc)
  {
    if (! ($server->{'data_source'} =~ /^([^:]*):([^:]+):([^:]*)/ ))
    {
      die "Can't find databasename in data_source: '" .
	  $server->{'data_source'}. "'\n";
    }
    if ($3) {
      $server->{'data_source'} = "$1:ODBC:$3";
    } else {
      $server->{'data_source'} = "$1:ODBC:$database";
    }
  }
  return $server;
}

sub all_servers
{
  return ["Access", "Adabas", "DB2", "Empress", "FrontBase", "Oracle",
	  "Informix", "InterBase", "Mimer", "mSQL", "MS-SQL", "MySQL", "Pg","SAPDB",
	  "Solid", "Sybase"];
}

#############################################################################
#	     First the configuration for MySQL off course :-)
#############################################################################

package db_MySQL;

sub new
{
  my ($type,$host,$database,$machine,$socket,$connect_options)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "mysql";
  $self->{'data_source'}	= "DBI:mysql:database=$database;host=$host";
  $self->{'data_source'} .= ";mysql_socket=$socket" if($socket);
  $self->{'data_source'} .= ";$connect_options" if($connect_options);
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "blob";
  $self->{'text'}		= "text";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'vacuum'}		= 1; # When using with --fast
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 0; # Transactions disabled by default

  $limits{'NEG'}		= 1; # Supports -id
  $limits{'alter_add_multi_col'}= 1; #Have ALTER TABLE t add a int,add b int;
  $limits{'alter_table'}	= 1; # Have ALTER TABLE
  $limits{'alter_table_dropcol'}= 1; # Have ALTER TABLE DROP column
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'func_extra_%'}	= 1; # Has % as alias for mod()
  $limits{'func_extra_if'}	= 1; # Have function if.
  $limits{'func_extra_in_num'}	= 1; # Has function in
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_odbc_mod'}	= 1; # Have function mod.
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_by_position'}  = 1; # Can use 'GROUP BY 1'
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'group_func_extra_std'} = 1; # Have group function std().
  $limits{'group_func_sql_min_str'} = 1; # Can execute MIN() and MAX() on strings
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'having_with_alias'}  = 1; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can use group functions in HAVING
  $limits{'insert_multi_value'} = 1; # Have INSERT ... values (1,2),(3,4)
  $limits{'insert_select'}	= 1;
  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'limit'}		= 1; # supports the limit attribute
  $limits{'truncate_table'}	= 1;
  $limits{'load_data_infile'}	= 1; # Has load data infile
  $limits{'lock_tables'}	= 1; # Has lock tables
  $limits{'max_column_name'}	= 64; # max table and column name
  $limits{'max_columns'}	= 2000;	# Max number of columns in table
  $limits{'max_conditions'}	= 9999; # (Actually not a limit)
  $limits{'max_index'}		= 16; # Max number of keys
  $limits{'max_index_parts'}	= 16; # Max segments/key
  $limits{'max_tables'}		= (($machine || '') =~ "^win") ? 5000 : 65000;
  $limits{'max_text_size'}	= 1000000; # Good enough for tests
  $limits{'multi_drop'}		= 1; # Drop table can take many tables
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'order_by_unused'}	= 1;
  $limits{'query_size'}		= 1000000; # Max size with default buffers.
  $limits{'select_without_from'}= 1; # Can do 'select 1';
  $limits{'subqueries'}		= 0; # Doesn't support sub-queries.
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'working_all_fields'} = 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  # Some fixes that depends on the environment
  if (defined($main::opt_create_options) &&
      $main::opt_create_options =~ /engine=heap/i)
  {
    $limits{'working_blobs'}	= 0; # HEAP tables can't handle BLOB's
  }
  if (defined($main::opt_create_options) &&
      $main::opt_create_options =~ /engine=innodb/i)
  {
    $self->{'transactions'}	= 1;	# Transactions enabled
  }
  if (defined($main::opt_create_options) &&
      $main::opt_create_options =~ /engine=ndb/i)
  {
    $self->{'transactions'}	= 1;	# Transactions enabled
    $limits{'max_columns'}	= 90;	# Max number of columns in table
    $limits{'max_tables'}	= 32;   # No comments
  }
  if (defined($main::opt_create_options) &&
      $main::opt_create_options =~ /engine=bdb/i)
  {
    $self->{'transactions'}	= 1;	# Transactions enabled
  }
  if (defined($main::opt_create_options) &&
      $main::opt_create_options =~ /engine=gemini/i)
  {
    $limits{'working_blobs'}	= 0; # Blobs not implemented yet
    $limits{'max_tables'}	= 500;
    $self->{'transactions'}	= 1;	# Transactions enabled
  }

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
  $sth = $dbh->prepare("select VERSION()") or die $DBI::errstr;
  $version="MySQL 3.20.?";
  if ($sth->execute && (@row = $sth->fetchrow_array))
  {
    $row[0] =~ s/-/ /g;			# To get better tables with long names
    $version="MySQL $row[0]";
  }
  $sth->finish;

  $sth = $dbh->prepare("show status like 'ssl_version'") or die $DBI::errstr;
  if ($sth->execute && (@row = $sth->fetchrow_array) && $row[1])
  {
    $version .= "/$row[1]";
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

#
# Connection with optional disabling of logging
#

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";

  $dbh->do("SET OPTION LOG_OFF=1,UPDATE_LOG=0");
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index,$options) = @_;
  my($query,@queries);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
#    $field =~ s/ decimal/ double(10,2)/i;
    $field =~ s/ big_decimal/ double(10,2)/i;
    $query.= $field . ',';
  }
  foreach $index (@$index)
  {
    $query.= $index . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  $query.=" $options" if (defined($options));
  $query.=" $main::opt_create_options" if (defined($main::opt_create_options));
  push(@queries,$query);
  return @queries;
}

sub insert_file {
  my ($self,$dbname, $file, $dbh) = @_;
  my ($command, $sth);

  $file =~ s|\\|/|g;			# Change Win32 names to Unix syntax
  $command = "load data infile '$file' into table $dbname columns optionally enclosed by '\\'' terminated by ','";
#  print "$command\n";
  $sth = $dbh->do($command) or die $DBI::errstr;
  return $sth;			# Contains number of rows
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index {
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index ON $table";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

#
# This should return 1 if we to do disconnect / connect when doing
# big batches
#

sub small_rollback_segment
{
  return 0;
}

#
# reconnect on errors (needed mainly be crash-me)
#

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#
# Optimize tables for better performance
#

sub vacuum
{
  my ($self,$full_vacuum,$dbh_ref,@tables)=@_;
  my ($loop_time,$end_time,$dbh);
  if ($#tables >= 0)
  {
    $dbh=$$dbh_ref;
    $loop_time=new Benchmark;
    $dbh->do("OPTIMIZE TABLE " . join(',',@tables)) || die "Got error: $DBI::errstr when executing 'OPTIMIZE TABLE'\n";
    $end_time=new Benchmark;
    print "Time for book-keeping (1): " .
      Benchmark::timestr(Benchmark::timediff($end_time, $loop_time),"all") . "\n\n";
  }
}

#############################################################################
#		     Definitions for mSQL
#############################################################################

package db_mSQL;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "msql";
  $self->{'data_source'}	= "DBI:mSQL:$database:$host";
  $self->{'limits'}		= \%limits;
  $self->{'double_quotes'}	= 0;
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 0;  # No transactions
  $self->{'blob'}		= "text(" . $limits{'max_text_size'} .")";
  $self->{'text'}		= "text(" . $limits{'max_text_size'} .")";

  $limits{'max_conditions'}	= 74;
  $limits{'max_columns'}	= 75;
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 32000;
  $limits{'query_size'}		= 65535;
  $limits{'max_index'}		= 5;
  $limits{'max_index_parts'}	= 10;
  $limits{'max_column_name'} = 35;

  $limits{'join_optimizer'}	= 0;		# Can't optimize FROM tables
  $limits{'load_data_infile'}	= 0;
  $limits{'lock_tables'}	= 0;
  $limits{'functions'}		= 0;
  $limits{'group_functions'}	= 0;
  $limits{'group_distinct_functions'}= 0;	 # Have count(distinct)
  $limits{'multi_drop'}		= 0;
  $limits{'select_without_from'}= 0;
  $limits{'subqueries'}		= 0;
  $limits{'left_outer_join'}	= 0;
  $limits{'table_wildcard'}	= 0;
  $limits{'having_with_alias'}  = 0;
  $limits{'having_with_group'}	= 0;
  $limits{'like_with_column'}	= 1;
  $limits{'order_by_position'}  = 1;
  $limits{'group_by_position'}  = 1;
  $limits{'alter_table'}	= 0;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;
  $limits{'group_func_extra_std'} = 0;
  $limits{'limit'}		= 1;		# supports the limit attribute
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 0;

  $limits{'func_odbc_mod'}	= 0;
  $limits{'func_extra_%'}	= 0;
  $limits{'func_odbc_floor'}	= 0;
  $limits{'func_extra_if'}	= 0;
  $limits{'column_alias'}	= 0;
  $limits{'NEG'}		= 0;
  $limits{'func_extra_in_num'}	= 0;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 
  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($tmp,$dir);
  foreach $dir ("/usr/local/Hughes", "/usr/local/mSQL","/my/local/mSQL",
		"/usr/local")
  {
    if (-x "$dir/bin/msqladmin")
    {
      $tmp=`$dir/bin/msqladmin version | grep server`;
      if ($tmp =~ /^\s*(.*\w)\s*$/)
      {				# Strip pre- and endspace
	$tmp=$1;
	$tmp =~ s/\s+/ /g;	# Remove unnecessary spaces
	$tmp .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);

	return $tmp;
      }
    }
  }
  return "mSQL version ???";
}


sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Can't handle many field types, so we map everything to int and real.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$name,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/varchar/char/i;		# mSQL doesn't have VARCHAR()
    # mSQL can't handle more than the real basic int types
    $field =~ s/tinyint|smallint|mediumint|integer/int/i;
    # mSQL can't handle different visual lengths
    $field =~ s/int\(\d*\)/int/i;
    # mSQL doesn't have float, change it to real
    $field =~ s/float(\(\d*,\d*\)){0,1}/real/i;
    $field =~ s/double(\(\d*,\d*\)){0,1}/real/i;
    # mSQL doesn't have blob, it has text instead
    if ($field =~ / blob/i)
    {
      $name=$self->{'blob'};
      $field =~ s/ blob/ $name/;
    }
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;

  # Prepend table_name to index name because the the name may clash with
  # a field name. (Should be diffent name space, but this is mSQL...)

  foreach $index (@$index)
  {
    # Primary key is unique index in mSQL
    $index =~ s/primary key/unique index primary/i;
    if ($index =~ /^unique\s*\(([^\(]*)\)$/i)
    {
      $nr++;
      push(@queries,"create unique index ${table_name}_$nr on $table_name ($1)");
    }
    else
    {
      if (!($index =~ /^(.*index)\s+(\w*)\s+(\(.*\))$/i))
      {
	die "Can't parse index information in '$index'\n";
      }
      push(@queries,"create $1 ${table_name}_$2 on $table_name $3");
    }
  }
  return @queries;
}


sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert an ascii file isn't supported by mSQL\n";
  return 0;
}


sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index FROM $table";
}

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#		     Definitions for PostgreSQL				    #
#############################################################################

package db_Pg;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "pg";
  $self->{'data_source'}	= "DBI:Pg:dbname=$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "text";
  $self->{'text'}		= "text";
  $self->{'double_quotes'}	= 1;
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{"vacuum"}		= 1;
  $limits{'join_optimizer'}	= 1;		# Can optimize FROM tables
  $limits{'load_data_infile'}	= 0;

  $limits{'NEG'}		= 1;
  $limits{'alter_add_multi_col'}= 0;		# alter_add_multi_col ?
  $limits{'alter_table'}	= 1;
  $limits{'alter_table_dropcol'}= 0;
  $limits{'column_alias'}	= 1;
  $limits{'func_extra_%'}	= 1;
  $limits{'func_extra_if'}	= 0;
  $limits{'func_extra_in_num'}	= 1;
  $limits{'func_odbc_floor'}	= 1;
  $limits{'func_odbc_mod'}	= 1;		# Has %
  $limits{'functions'}		= 1;
  $limits{'group_by_position'}  = 1;
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'group_func_extra_std'} = 0;
  $limits{'group_func_sql_min_str'}= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_functions'}	= 1;
  $limits{'having_with_alias'}  = 0;
  $limits{'having_with_group'}	= 1;
  $limits{'insert_select'}	= 1;
  $limits{'left_outer_join'}	= 1;
  $limits{'like_with_column'}	= 1;
  $limits{'lock_tables'}	= 0;		# in ATIS gives this a problem
  $limits{'max_column_name'} 	= 128;
  $limits{'max_columns'}	= 1000;		# 500 crashes pg 6.3
  $limits{'max_conditions'}	= 9999;		# This makes Pg real slow
  $limits{'max_index'}		= 64;		# Big enough
  $limits{'max_index_parts'}	= 16;
  $limits{'max_tables'}		= 5000;		# 10000 crashes pg 7.0.2
  $limits{'max_text_size'}	= 65000;	# Good enough for test
  $limits{'multi_drop'}		= 1;
  $limits{'order_by_position'}  = 1;
  $limits{'order_by_unused'}	= 1;
  $limits{'query_size'}		= 16777216;
  $limits{'select_without_from'}= 1;
  $limits{'subqueries'}		= 1;
  $limits{'table_wildcard'}	= 1;
  $limits{'truncate_table'}	= 1;
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'working_all_fields'} = 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

# couldn't find the option to get the version number

sub version
{
  my ($version,$dir);
  $version = "PostgreSQL version ???";
  foreach $dir ($ENV{'PGDATA'},"/usr/local/pgsql/data", "/usr/local/pg/data")
  {
    if ($dir && -e "$dir/PG_VERSION")
    {
      $version= `cat $dir/PG_VERSION`;
      if ($? == 0)
      {
	chomp($version);
	$version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
	return "PostgreSQL $version";
      }
    }
  }
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}


sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}


sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$name,$in,$indfield,$table,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    if ($main::opt_fast)
    {
      # Allow use of char2, char4, char8 or char16
      $field =~ s/char(2|4|8|16)/char$1/;
    }
    # Pg can't handle more than the real basic int types
    $field =~ s/tinyint|smallint|mediumint|integer/int/;
    # Pg can't handle different visual lengths
    $field =~ s/int\(\d*\)/int/;
    $field =~ s/float\(\d*,\d*\)/float/;
    $field =~ s/ double/ float/;
#    $field =~ s/ decimal/ float/i;
#    $field =~ s/ big_decimal/ float/i;
#    $field =~ s/ date/ int/i;
    # Pg doesn't have blob, it has text instead
    $field =~ s/ blob/ text/;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  foreach $index (@$index)
  {
    $index =~ s/primary key/unique index primary_key/i;
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      # original: $indfield="using btree (" .$1.")";
      # using btree doesn´t seem to work with Postgres anymore; it creates
      # the table and adds the index, but it isn´t unique
      $indfield=" (" .$1.")";	
      $in="unique index";
      $table="index_$nr"; $nr++;
    }
    elsif ($index =~ /^(.*index)\s+(\w*)\s+(\(.*\))$/i)
    {
      # original: $indfield="using btree (" .$1.")";
      $indfield=" " .$3;
      $in="index";
      $table="index_$nr"; $nr++;
    }
    else
    {
      die "Can't parse index information in '$index'\n";
    }
    push(@queries,"create $in ${table_name}_$table on $table_name $indfield");
  }
  $queries[0]=$query;
  return @queries;
}

sub insert_file {
  my ($self,$dbname, $file, $dbh) = @_;
  my ($command, $sth);

# Syntax:
# copy [binary] <class_name> [with oids]
#      {to|from} {<filename>|stdin|stdout} [using delimiters <delim>]
  print "The ascii files aren't correct for postgres ....!!!\n";
  $command = "copy $dbname from '$file' using delimiters ','";
  print "$command\n";
  $sth = $dbh->do($command) or die $DBI::errstr;
  return $sth;
}

#
# As postgreSQL wants A % B instead of standard mod(A,B) we have to map
# This will not handle all cases, but as the benchmarks doesn't use functions
# inside MOD() the following should work
#
# PostgreSQL cant handle count(*) or even count(1), but it can handle
# count(1+1) sometimes. ==> this is solved in PostgreSQL 6.3
#
# PostgreSQL 6.5 is supporting MOD.

sub query {
  my($self,$sql) = @_;
  my(@select,$change);
# if you use PostgreSQL 6.x and x is lower as 5 then uncomment the line below.
#  $sql =~ s/mod\(([^,]*),([^\)]*)\)/\($1 % $2\)/gi;
#
# if you use PostgreSQL 6.1.x uncomment the lines below
#  if ($sql =~ /select\s+count\(\*\)\s+from/i) {
#  }
#  elsif ($sql =~ /count\(\*\)/i)
#  {
#    if ($sql =~ /select\s+(.*)\s+from/i)
#    {
#      @select = split(/,/,$1);
#      if ($select[0] =~ /(.*)\s+as\s+\w+$/i)
#      {
# 	$change = $1;
#      }
#      else
#      {
#	$change = $select[0];
#      }
#    }
#    if (($change =~ /count/i) || ($change eq "")) {
#      $change = "1+1";
#    }
#    $sql =~ s/count\(\*\)/count($change)/gi;
#  }
# till here.
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

sub abort_if_fatal_error
{
  return 1 if ($DBI::errstr =~ /sent to backend, but backend closed/i);
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

sub vacuum
{
  my ($self,$full_vacuum,$dbh_ref,@tables)=@_;
  my ($loop_time,$end_time,$dbh,$table);
  if (defined($full_vacuum))
  {
    $$dbh_ref->disconnect;  $$dbh_ref= $self->connect();
  }
  $dbh=$$dbh_ref;
  $loop_time=new Benchmark;
  if ($#tables >= 0)
  {
    foreach $table (@tables)
    {
      $dbh->do("vacuum analyze $table") || die "Got error: $DBI::errstr when executing 'vacuum analyze $table'\n";
      $dbh->do("vacuum $table") || die "Got error: $DBI::errstr when executing 'vacuum'\n";
    }
  }
  else
  {
#    $dbh->do("vacuum pg_attributes") || die "Got error: $DBI::errstr when executing 'vacuum'\n";
#    $dbh->do("vacuum pg_index") || die "Got error: $DBI::errstr when executing 'vacuum'\n";
    $dbh->do("vacuum analyze") || die "Got error: $DBI::errstr when executing 'vacuum analyze'\n";
    $dbh->do("vacuum") || die "Got error: $DBI::errstr when executing 'vacuum'\n";
  }
  $end_time=new Benchmark;
  print "Time for book-keeping (1): " .
  Benchmark::timestr(Benchmark::timediff($end_time, $loop_time),"all") . "\n\n";
  $dbh->disconnect;  $$dbh_ref= $self->connect();
}


#############################################################################
#		     Definitions for Solid
#############################################################################

package db_Solid;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "solid";
  $self->{'data_source'}	= "DBI:Solid:";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "long varchar";
  $self->{'text'}		= "long varchar";
  $self->{'double_quotes'}	= 1;
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 9999;		# Probably big enough
  $limits{'max_columns'}	= 2000;		# From crash-me
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 65492;	# According to tests
  $limits{'query_size'}		= 65535;	# Probably a limit
  $limits{'max_index'}		= 64;		# Probably big enough
  $limits{'max_index_parts'}	= 64;
  $limits{'max_column_name'} = 80;

  $limits{'join_optimizer'}	= 1;
  $limits{'load_data_infile'}	= 0;
  $limits{'lock_tables'}	= 0;
  $limits{'functions'}		= 1;
  $limits{'group_functions'}	= 1;
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0;		# Can do 'select 1' ?;
  $limits{'multi_drop'}		= 0;
  $limits{'subqueries'}		= 1;
  $limits{'left_outer_join'}	= 1;
  $limits{'table_wildcard'}	= 1;
  $limits{'having_with_alias'}  = 0;
  $limits{'having_with_group'}	= 1;
  $limits{'like_with_column'}	= 1;
  $limits{'order_by_position'}  = 0;		# 2.30.0018 can this
  $limits{'group_by_position'}  = 0;
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'}	= 0;	# Have group function std().

  $limits{'func_odbc_mod'}	= 1;
  $limits{'func_extra_%'}	= 0;
  $limits{'func_odbc_floor'}	= 1;
  $limits{'column_alias'}	= 1;
  $limits{'NEG'}		= 1;
  $limits{'func_extra_in_num'}	= 1;
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($version,$dir);
  $version="Solid version ??";
  foreach $dir ($ENV{'SOLIDDIR'},"/usr/local/solid", "/my/local/solid")
  {
    if ($dir && -e "$dir/bin/solcon")
    {
      $version=`$dir/bin/solcon -e"ver" $main::opt_user $main::opt_password | grep Server | sed q`;
      if ($? == 0)
      {
	chomp($version);
	$version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
	return $version;
      }
    }
  }
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/integer/i;
    $field =~ s/ double/ float/i;
    # Solid doesn't have blob, it has long varchar
    $field =~ s/ blob/ long varchar/;
#    $field =~ s/ decimal/ float/i;
#    $field =~ s/ big_decimal/ float/i;
#    $field =~ s/ date/ int/i;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;
  foreach $index (@$index)
  {
    if ($index =~ /^primary key/i || $index =~ /^unique/i)
    {					# Add to create statement
      substr($queries[0],-1,0)="," . $index;
    }
    else
    {
      $index =~ /^(.*)\s+(\(.*\))$/;
      push(@queries,"create ${1}$nr on $table_name $2");
      $nr++;
    }
  }
  return @queries;
}

# there is no sql statement in solid which can do the load from
# an ascii file in the db ... but there is the speedloader program
# an external program which can load the ascii file in the db ...
# the server must be down before using speedloader !!!!
# (in the standalone version)
# it works also with a control file ... that one must be made ....
sub insert_file {
  my ($self, $dbname, $file) = @_;
  my ($speedcmd);
  $speedcmd = '/usr/local/solid/bin/solload';
  print "At this moment not supported - solid server must go down \n";
  return 0;
}

# solid can't handle an alias in a having statement so
# select test as foo from tmp group by foo having foor > 2
# becomes
# select test as foo from tmp group by foo having test > 2
#
sub query {
  my($self,$sql) = @_;
  my(@select,$tmp,$newhaving,$key,%change);

  if ($sql =~ /having\s+/i)
  {
    if ($sql =~ /select (.*) from/i)
    {
      (@select) = split(/,\s*/, $1);
      foreach $tmp (@select)
      {
	if ($tmp =~ /(.*)\s+as\s+(\w+)/)
	{
	  $change{$2} = $1;
	}
      }
    }
    if ($sql =~ /having\s+(\w+)/i)
    {
      $newhaving = $1;
      foreach $key (sort {$a cmp $b} keys %change)
      {
	if ($newhaving eq $key)
	{
	  $newhaving =~ s/$key/$change{$key}/g;
	}
      }
    }
    $sql =~ s/(having)\s+(\w+)/$1 $newhaving/i;
  }
  return $sql;
}


sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

sub reconnect_on_errors
{
  return 0;
}

#############################################################################
#		     Definitions for Empress
#
# at this moment DBI:Empress can only handle 200 prepare statements ...
# so Empress can't be tested with the benchmark test :(
#############################################################################

package db_Empress;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "empress";
  $self->{'data_source'}        = "DBI:EmpressNet:SERVER=$host;Database=/usr/local/empress/rdbms/bin/$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "text";
  $self->{'text'}		= "text";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 1258;
  $limits{'max_columns'}	= 226;		# server is disconnecting????
			# above this value .... but can handle 2419 columns
			# maybe something for crash-me ... but how to check ???
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 4095;		# max returned ....
  $limits{'query_size'}		= 65535;	# Not a limit, big enough
  $limits{'max_index'}		= 64;		# Big enough
  $limits{'max_index_parts'}	= 64;		# Big enough
  $limits{'max_column_name'} 	= 31;

  $limits{'join_optimizer'}	= 1;
  $limits{'load_data_infile'}	= 0;
  $limits{'lock_tables'}	= 1;
  $limits{'functions'}		= 1;
  $limits{'group_functions'}	= 1;
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0;
  $limits{'multi_drop'}		= 0;
  $limits{'subqueries'}		= 1;
  $limits{'table_wildcard'}	= 0;
  $limits{'having_with_alias'}  = 0; 	# AS isn't supported in a select
  $limits{'having_with_group'}	= 1;
  $limits{'like_with_column'}	= 1;
  $limits{'order_by_position'}  = 1;
  $limits{'group_by_position'}  = 0;
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'}= 0;	# Have group function std().

  $limits{'func_odbc_mod'}	= 0;
  $limits{'func_extra_%'}	= 1;
  $limits{'func_odbc_floor'}	= 1;
  $limits{'func_extra_if'}	= 0;
  $limits{'column_alias'}	= 0;
  $limits{'NEG'}		= 1;
  $limits{'func_extra_in_num'}	= 0;
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self,$dbh)=@_;
  my ($version);
  $version="";
  if (-x "/usr/local/empress/rdbms/bin/empvers")
  {
    $version=`/usr/local/empress/rdbms/bin/empvers | grep Version`;
  }
  if ($version)
  {
    chomp($version);
  }
  else
  {
    $version="Empress version ???";
  }

  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  my($command,$sth);
  $command = "insert into $dbname from '$file'";
  print "$command\n" if ($opt_debug);
  $sth = $dbh->do($command) or die $DBI::errstr;

  return $sth;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/int/i;
    $field =~ s/tinyint/int/i;
    $field =~ s/smallint/int/i;
    $field =~ s/longint/int/i;
    $field =~ s/integer/int/i;
    $field =~ s/ double/ longfloat/i;
    # Solid doesn't have blob, it has long varchar
#    $field =~ s/ blob/ text(65535,65535,65535,65535)/;
    $field =~ s/ blob/ text/;
    $field =~ s/ varchar\((\d+)\)/ char($1,3)/;
    $field =~ s/ char\((\d+)\)/ char($1,3)/;
#    $field =~ s/ decimal/ float/i;
#    $field =~ s/ big_decimal/ longfloat/i;
#    $field =~ s/ date/ int/i;
    $field =~ s/ float(.*)/ float/i;
    if ($field =~ / int\((\d+)\)/) {
      if ($1 > 4) {
        $field =~ s/ int\(\d+\)/ longinteger/i;
      } else {
        $field =~ s/ int\(\d+\)/ longinteger/i;
      }
    } else {
      $field =~ s/ int/ longinteger/i;
    }
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=1;
  foreach $index (@$index)
  {
    # Primary key is unique index in Empress
    $index =~ s/primary key/unique index/i;
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      $nr++;
      push(@queries,"create unique index ${table_name}_$nr on $table_name ($1)");
    }
    else
    {
      if (!($index =~ /^(.*index)\s+(\w*)\s+(\(.*\))$/i))
      {
	die "Can't parse index information in '$index'\n";
      }
      push(@queries,"create $1 ${table_name}_$2 on $table_name $3");
    }
  }
  return @queries;
}

# empress can't handle an alias and but can handle the number of the
# columname - so
# select test as foo from tmp order by foo
# becomes
# select test from tmp order by 1
#
sub query {
  my($self,$sql) = @_;
  my(@select,$i,$tmp,$newselect,$neworder,@order,$key,%change);
  my($tmp1,$otmp,$tmp2);

  if ($sql =~ /\s+as\s+/i)
  {
    if ($sql =~ /select\s+(.*)\s+from/i) {
      $newselect = $1;
      (@select) = split(/,\s*/, $1);
      $i = 1;
      foreach $tmp (@select) {
	if ($tmp =~ /\s+as\s+(\w+)/) {
	  $change{$1} = $i;
	}
	$i++;
      }
    }
    $newselect =~ s/\s+as\s+(\w+)//gi;
    $tmp2 = 0;
    if ($sql =~ /order\s+by\s+(.*)$/i) {
      (@order) = split(/,\s*/, $1);
      foreach $otmp (@order) {
	foreach $key (sort {$a cmp $b} keys %change) {
	  if ($otmp eq $key) {
	    $neworder .= "$tmp1"."$change{$key}";
	    $tmp1 = ", ";
	    $tmp2 = 1;
	  } elsif ($otmp =~ /(\w+)\s+(.+)$/) {
	    if ($key eq $1) {
	      $neworder .= "$tmp1"."$change{$key} $2";
	      $tmp2 = 1;
	    }
	  }
	}
	if ($tmp2 == 0) {
	  $neworder .= "$tmp1"."$otmp";
	}
	$tmp2 = 0;
	$tmp1 = ", ";
      }
    }
    $sql =~ s/(select)\s+(.*)\s+(from)/$1 $newselect $3/i;
    $sql =~ s/(order\s+by)\s+(.*)$/$1 $neworder/i;
  }
  return $sql;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  $cmd =~ s/\'\'/\' \'/g;
  return $cmd;
}


sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

# This is a because of the 200 statement problem with DBI-Empress

sub abort_if_fatal_error
{
  if ($DBI::errstr =~ /Overflow of table of prepared statements/i)
  {
    print "Overflow of prepared statements ... killing the process\n";
    exit 1;
  }
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

#############################################################################
#	                 Definitions for Oracle
#############################################################################

package db_Oracle;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "Oracle";
  $self->{'data_source'}	= "DBI:Oracle:$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "long";
  $self->{'text'}		= "long";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{"vacuum"}		= 1;

  $limits{'max_conditions'}	= 9999; # (Actually not a limit)
  $limits{'max_columns'}	= 254;	# Max number of columns in table
  $limits{'max_tables'}		= 65000; # Should be big enough
  $limits{'max_text_size'}	= 2000; # Limit for blob test-connect
  $limits{'query_size'}		= 65525; # Max size with default buffers.
  $limits{'max_index'}		= 16; # Max number of keys
  $limits{'max_index_parts'}	= 16; # Max segments/key
  $limits{'max_column_name'} = 32; # max table and column name

  $limits{'truncate_table'}	= 1;
  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0;
  $limits{'multi_drop'}		= 0;
  $limits{'subqueries'}		= 1;
  $limits{'left_outer_join'}	= 0; # This may be fixed in the query module
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can't use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0;
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'}	= 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 0; # Oracle has problem with mod()
  $limits{'func_extra_%'}	= 0; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 1; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 


  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
  $sth = $dbh->prepare("select VERSION from product_component_version WHERE PRODUCT like 'Oracle%'") or die $DBI::errstr;
  $version="Oracle 7.x";
  if ($sth->execute && (@row = $sth->fetchrow_array))
  {
    $version="Oracle $row[0]";
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$ind,@keys);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/ character\((\d+)\)/ char\($1\)/i;
    $field =~ s/ character varying\((\d+)\)/ varchar\($1\)/i;
    $field =~ s/ char varying\((\d+)\)/ varchar\($1\)/i;
    $field =~ s/ integer/ number\(38\)/i;
    $field =~ s/ int/ number\(38\)/i;
    $field =~ s/ tinyint/ number\(38\)/i;
    $field =~ s/ smallint/ number\(38\)/i;
    $field =~ s/ mediumint/ number\(38\)/i;
    $field =~ s/ tinynumber\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ smallnumber\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ mediumnumber\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ number\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ numeric\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ decimal\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ dec\((\d+)\)\((\d+)\)/ number\($1,$2\)/i;
    $field =~ s/ float/ number/;
    $field =~ s/ real/ number/;
    $field =~ s/ double precision/ number/;
    $field =~ s/ double/ number/;
    $field =~ s/ blob/ long/;
    $query.= $field . ',';
  }

  foreach $ind (@$index)
  {
    my @index;
    if ( $ind =~ /\bKEY\b/i ){
      push(@keys,"ALTER TABLE $table_name ADD $ind");
    }else{
      my @fields = split(' ',$index);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@index,$query);
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query,@keys,@index);
#print "query:$query\n";

  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert an ascii file isn't supported by Oracle (?)\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  $cmd =~ s/\'\'/\' \'/g;
  return $cmd;
}


sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 1;
}

sub reconnect_on_errors
{
  return 0;
}

#
# optimize the tables ....
#
sub vacuum
{
  my ($self,$full_vacuum,$dbh_ref)=@_;
  my ($loop_time,$end_time,$sth,$dbh);

  if (defined($full_vacuum))
  {
    $$dbh_ref->disconnect;  $$dbh_ref= $self->connect();
  }
  $dbh=$$dbh_ref;
  $loop_time=new Benchmark;
  # first analyze all tables
  $sth = $dbh->prepare("select table_name from user_tables") || die "Got error: $DBI::errstr";
  $sth->execute || die "Got error: $DBI::errstr when select user_tables";
  while (my @r = $sth->fetchrow_array)
  {
    $dbh->do("analyze table $r[0] compute statistics") || die "Got error: $DBI::errstr when executing 'analyze table'\n";
  }
  # now analyze all indexes ...
  $sth = $dbh->prepare("select index_name from user_indexes") || die "Got error: $DBI::errstr";
  $sth->execute || die "Got error: $DBI::errstr when select user_indexes";
  while (my @r1 = $sth->fetchrow_array)
  {
    $dbh->do("analyze index $r1[0] compute statistics") || die "Got error: $DBI::errstr when executing 'analyze index $r1[0]'\n";
  }
  $end_time=new Benchmark;
  print "Time for book-keeping (1): " .
  Benchmark::timestr(Benchmark::timediff($end_time, $loop_time),"all") . "\n\n";
  $dbh->disconnect;  $$dbh_ref= $self->connect();
}


#############################################################################
#	                 Definitions for Informix
#############################################################################

package db_Informix;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "Informix";
  $self->{'data_source'}	= "DBI:Informix:$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "byte in table";
  $self->{'text'}		= "byte in table";
  $self->{'double_quotes'}	= 0; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{'host'}		= $host;

  $limits{'NEG'}		= 1; # Supports -id
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 1;
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'func_extra_%'}	= 0; # Has % as alias for mod()
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'func_extra_in_num'}= 0; # Has function in
  $limits{'func_odbc_floor'}	= 0; # Has func_odbc_floor function
  $limits{'func_odbc_mod'}	= 1; # Have function mod.
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_by_position'}  = 1; # Can use 'GROUP BY 1'
  $limits{'group_by_alias'}  = 0; # Can use 'select a as ab from x GROUP BY ab'
  $limits{'group_func_extra_std'} = 0; # Have group function std().
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}= 1; # Can't use group functions in HAVING
  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables (always 1 only for msql)
  $limits{'left_outer_join'}	= 0; # Supports left outer joins (ANSI)
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 1; # Has lock tables
  $limits{'max_conditions'}	= 1214; # (Actually not a limit)
  $limits{'max_column_name'}	= 18; # max table and column name
  $limits{'max_columns'}	= 994;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_index'}		= 64; # Max number of keys
  $limits{'max_index_parts'}	= 15; # Max segments/key
  $limits{'max_text_size'}	= 65535;  # Max size with default buffers. ??
  $limits{'multi_drop'}		= 0; # Drop table can take many tables
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'query_size'}		= 32766; # Max size with default buffers.
  $limits{'select_without_from'}= 0; # Can do 'select 1';
  $limits{'subqueries'}		= 1; # Doesn't support sub-queries.
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $ENV{'INFORMIXSERVER'} = $self->{'host'};
  $dbh=$self->connect();
  $sth = $dbh->prepare("SELECT owner FROM systables WHERE tabname = ' VERSION'")
						      or die $DBI::errstr;
  $version='Informix unknown';
  if ($sth->execute && (@row = $sth->fetchrow_array))
  {
    $version="Informix $row[0]";
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}


#
# Create table
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$name,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
#    $field =~ s/\btransport_description\b/transport_desc/;
				# to overcome limit 18 chars
    $field =~ s/tinyint/smallint/i;
    $field =~ s/tinyint\(\d+\)/smallint/i;
    $field =~ s/mediumint/integer/i;
    $field =~ s/mediumint\(\d+\)/integer/i;
    $field =~ s/smallint\(\d+\)/smallint/i;
    $field =~ s/integer\(\d+\)/integer/i;
    $field =~ s/int\(\d+\)/integer/i;
#    $field =~ s/\b(?:small)?int(?:eger)?\((\d+)\)/decimal($1)/i;
#    $field =~ s/float(\(\d*,\d*\)){0,1}/real/i;
    $field =~ s/(float|double)(\(.*?\))?/float/i;

    if ($field =~ / blob/i)
    {
      $name=$self->{'blob'};
      $field =~ s/ blob/ $name/;
    }
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;

  foreach $index (@$index)
  {
    # Primary key is unique index in Informix
    $index =~ s/primary key/unique index primary/i;
    if ($index =~ /^unique\s*\(([^\(]*)\)$/i)
    {
      $nr++;
      push(@queries,"create unique index ${table_name}_$nr on $table_name ($1)");
    }
    else
    {
      if (!($index =~ /^(.*index)\s+(\w*)\s+(\(.*\))$/i))
      {
	die "Can't parse index information in '$index'\n";
      }
      ### push(@queries,"create $1 ${table_name}_$2 on $table_name $3");
      $nr++;
      push(@queries,"create $1 ${table_name}_$nr on $table_name $3");
    }
  }
  return @queries;
}
#
# Some test needed this
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}


sub fix_for_insert
{
  my ($self,$cmd) = @_;
  $cmd =~ s/\\\'//g;
  return $cmd;
}



sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}


#############################################################################
#	     Configuration for Access
#############################################################################

package db_access;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "access";
  $self->{'data_source'}	= "DBI:ODBC:$database";
  if (defined($host) && $host ne "")
  {
    $self->{'data_source'}	.= ":$host";
  }
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "blob";
  $self->{'text'}		= "blob"; # text ? 
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 97; # We get 'Query is too complex'
  $limits{'max_columns'}	= 255;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 255;  # Max size with default buffers.
  $limits{'query_size'}		= 65535; # Not a limit, big enough
  $limits{'max_index'}		= 32; # Max number of keys
  $limits{'max_index_parts'}	= 10; # Max segments/key
  $limits{'max_column_name'}	= 64; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 0; # Have count(distinct)
  $limits{'select_without_from'}= 1; # Can do 'select 1';
  $limits{'multi_drop'}		= 0; # Drop table can take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Can use 'GROUP BY 1'
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 2; #Have ALTER TABLE t add a int, b int;
  $limits{'alter_table_dropcol'}= 1;

  $limits{'group_func_extra_std'} = 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod.
  $limits{'func_extra_%'}	= 0; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 0; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 1; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 
  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my $version="Access 2000";
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;		#DBI/ODBC can't return the server version
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/integer/i;
    $field =~ s/tinyint/smallint/i;
    $field =~ s/float\(\d+,\d+\)/float/i;
    $field =~ s/integer\(\d+\)/integer/i;
    $field =~ s/smallint\(\d+\)/smallint/i;
    $field =~ s/int\(\d+\)/integer/i;
    $field =~ s/blob/text/i;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;
  foreach $index (@$index)
  {
    $ext="WITH DISALLOW NULL";
    if (($index =~ s/primary key/unique index primary_key/i))
    {
      $ext="WITH PRIMARY;"
    }
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      $nr++;
      $index="unique index ${table_name}_$nr ($1)";
    }
    $index =~ /^(.*)\s+(\(.*\))$/;
    push(@queries,"create ${1} on $table_name $2");
  }
  return @queries;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index ON $table";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 1 if (($DBI::errstr =~ /The database engine couldn\'t lock table/i) ||
               ($DBI::errstr =~ /niet vergrendelen. De tabel is momenteel in gebruik /i) ||
	       ($DBI::errstr =~ /Den anv.* redan av en annan/i) ||
	       ($DBI::errstr =~ /non-exclusive access/));
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 1;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for Microsoft SQL server
#############################################################################

package db_ms_sql;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "ms-sql";
  $self->{'data_source'}	= "DBI:ODBC:$database";
  if (defined($host) && $host ne "")
  {
    $self->{'data_source'}	.= ":$host";
  }
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "text";
  $self->{'text'}		= "text";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 1030; # We get 'Query is too complex'
  $limits{'max_columns'}	= 250;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 9830;  # Max size with default buffers.
  $limits{'query_size'}		= 9830; # Max size with default buffers.
  $limits{'max_index'}		= 64; # Max number of keys
  $limits{'max_index_parts'}	= 15; # Max segments/key
  $limits{'max_column_name'}	= 30; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 1; # Can do 'select 1';
  $limits{'multi_drop'}		= 1; # Drop table can take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can't use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Can use 'GROUP BY 1'
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'} = 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod.
  $limits{'func_extra_%'}	= 1; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 0; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 
  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my($sth,@row, $version);
  $version='MS SQL server ?';
  $dbh=$self->connect();
  $sth = $dbh->prepare("SELECT \@\@VERSION") or die $DBI::errstr;
  $sth->execute or die $DBI::errstr;
  @row = $sth->fetchrow_array;
  if ($row[0]) {
     @server = split(/\n/,$row[0]);
     chomp(@server);
     $version= "$server[0]";
  } 
  $sth->finish;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/integer/i;
    $field =~ s/float\(\d+,\d+\)/float/i;
    $field =~ s/double\(\d+,\d+\)/float/i;
    $field =~ s/double/float/i;
    $field =~ s/integer\(\d+\)/integer/i;
    $field =~ s/int\(\d+\)/integer/i;
    $field =~ s/smallint\(\d+\)/smallint/i;
    $field =~ s/smallinteger/smallint/i;
    $field =~ s/tinyint\(\d+\)/tinyint/i;
    $field =~ s/tinyinteger/tinyint/i;
    $field =~ s/blob/text/i;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;
  foreach $index (@$index)
  {
    $ext="WITH DISALLOW NULL";
    if (($index =~ s/primary key/unique index primary_key/i))
    {
      $ext="WITH PRIMARY;"
    }
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      $nr++;
      $index="unique index ${table_name}_$nr ($1)";
    }
    $index =~ /^(.*)\s+(\(.*\))$/;
    push(@queries,"create ${1} on $table_name $2");
  }
  return @queries;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $table.$index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for Sybase
#############################################################################
package db_sybase;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "sybase";
  $self->{'data_source'}	= "DBI:Sybase:database=$database";
  if (defined($host) && $host ne "")
  {
    $self->{'data_source'}	.= ";hostname=$host";
  }
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "text";
  $self->{'text'}		= "text";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{"vacuum"}		= 1;

  $limits{'max_conditions'}	= 1030; # We get 'Query is too complex'
  $limits{'max_columns'}	= 250;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 9830;  # Max size with default buffers.
  $limits{'query_size'}		= 9830; # Max size with default buffers.
  $limits{'max_index'}		= 64; # Max number of keys
  $limits{'max_index_parts'}	= 15; # Max segments/key
  $limits{'max_column_name'}	= 30; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 1; # Can do 'select 1';
  $limits{'multi_drop'}		= 1; # Drop table can take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can't use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Can use 'GROUP BY 1'
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'} = 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod.
  $limits{'func_extra_%'}	= 1; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 0; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 
  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
  $sth = $dbh->prepare('SELECT @@version') or die $DBI::errstr;
  $version="Sybase (unknown)";
  if ($sth->execute && (@row = $sth->fetchrow_array))
  {
    $version=$row[0];
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0 , AutoCommit => 1}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/integer/i;
    $field =~ s/float\(\d+,\d+\)/float/i;
    $field =~ s/int\(\d+\)/int/i;
    $field =~ s/double/float/i;
    $field =~ s/integer\(\d+\)/integer/i;
    $field =~ s/smallint\(\d+\)/smallint/i;
    $field =~ s/tinyint\(\d+\)/tinyint/i;
    $field =~ s/blob/text/i;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;
  foreach $index (@$index)
  {
#    $ext="WITH DISALLOW NULL";
    if (($index =~ s/primary key/unique index primary_key/i))
    {
#      $ext="WITH PRIMARY;"
    }
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      $nr++;
      $index="unique index ${table_name}_$nr ($1)";
    }
    $index =~ /^(.*)\s+(\(.*\))$/;
    push(@queries,"create ${1} on $table_name $2");
  }
  return @queries;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $table.$index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#
# optimize the tables ....
#  WARNING (from walrus)! This sub will work only from DBD:sybase
# driver. Because if we use ODBC we don't know actual database name
# (but DSN name only)
sub vacuum
{
  my ($self,$full_vacuum,$dbh_ref)=@_;
  my ($loop_time,$end_time,$dbh);

  if (defined($full_vacuum))
  {
    $$dbh_ref->disconnect;  $$dbh_ref= $self->connect();
  }
  $dbh=$$dbh_ref;
  $loop_time=new Benchmark;
  my (@tables,$sth,$current_table,$current_base);
  $dbh->do("dump tran $database with truncate_only");
  $sth=$dbh->prepare("sp_tables" ) or die "prepere";
  $sth->execute() or die "execute";
  while (@row = $sth->fetchrow_array()) {
    $current_table = $row[2];
    $current_base = $row[0];
    next if ($current_table =~ /^sys/); 
    push(@tables,$current_table) if ($database == $current_base);    
   }

  $sth->finish();

  foreach $table (@tables) {
#    print "$table: \n";
    $dbh->do("update statistics $table") or print "Oops!"; 
  }
 
#  $dbh->do("analyze table ?? compute statistics") || die "Got error: $DBI::errstr when executing 'vacuum'\n";
  $end_time=new Benchmark;
  print "Time for book-keeping (1): " .
  Benchmark::timestr(Benchmark::timediff($end_time, $loop_time),"all") . "\n\n";
  $dbh->disconnect;  $$dbh_ref= $self->connect();
}




#############################################################################
#	                 Definitions for Adabas
#############################################################################

package db_Adabas;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "Adabas";
  $self->{'data_source'}	= "DBI:Adabas:$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "long";
  $self->{'text'}		= "long";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 50; # (Actually not a limit)
  $limits{'max_columns'}	= 254;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 2000; # Limit for blob test-connect
  $limits{'query_size'}		= 65525; # Max size with default buffers.
  $limits{'max_index'}		= 16; # Max number of keys
  $limits{'max_index_parts'}	= 16; # Max segments/key
  $limits{'max_column_name'} = 32; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}	= 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0;
  $limits{'multi_drop'}		= 0;
  $limits{'subqueries'}		= 1;
  $limits{'left_outer_join'}	= 0; # This may be fixed in the query module
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can't use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 1;
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 2; #Have ALTER TABLE t add a int, b int;
  $limits{'alter_table_dropcol'}= 1;

  $limits{'group_func_extra_std'}	= 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 0; # Oracle has problem with mod()
  $limits{'func_extra_%'}	= 0; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 1; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 


  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
  $sth = $dbh->prepare("SELECT KERNEL FROM VERSIONS") or die $DBI::errstr;
  $version="Adabas (unknown)";
  if ($sth->execute && (@row = $sth->fetchrow_array)
      && $row[0] =~ /([\d\.]+)/)
  {
    $version="Adabas $1";
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$ind,@keys);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/CHARACTER\s+VARYING/VARCHAR/i;
    $field =~ s/TINYINT/SMALLINT/i;
    $field =~ s/MEDIUMINT/INT/i;
    $field =~ s/SMALLINT\s*\(\d+\)/SMALLINT/i;
    $field =~ s/INT\s*\(\d+\)/INT/i;
    $field =~ s/BLOB/LONG/i;
    $field =~ s/INTEGER\s*\(\d+\)/INTEGER/i;
    $field =~ s/FLOAT\s*\((\d+),\d+\)/FLOAT\($1\)/i;
    $field =~ s/DOUBLE/FLOAT\(38\)/i;
    $field =~ s/DOUBLE\s+PRECISION/FLOAT\(38\)/i;
    $query.= $field . ',';
  }

  foreach $ind (@$index)
  {
    my @index;
    if ( $ind =~ /\bKEY\b/i ){
      push(@keys,"ALTER TABLE $table_name ADD $ind");
    }else{
      my @fields = split(' ',$index);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@index,$query);
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query,@keys,@index);
#print "query:$query\n";

  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert an ascii file isn't supported by Oracle (?)\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for IBM DB2
#############################################################################

package db_db2;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "DB2";
  $self->{'data_source'}	= "DBI:ODBC:$database";
  if (defined($host) && $host ne "")
  {
    $self->{'data_source'}	.= ":$host";
  }
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "varchar(255)";
  $self->{'text'}		= "varchar(255)";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled

  $limits{'max_conditions'}	= 418; # We get 'Query is too complex'
  $limits{'max_columns'}	= 500;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 254;  # Max size with default buffers.
  $limits{'query_size'}		= 254; # Max size with default buffers.
  $limits{'max_index'}		= 48; # Max number of keys
  $limits{'max_index_parts'}	= 15; # Max segments/key
  $limits{'max_column_name'}	= 18; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'}= 1;
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0; # Can do 'select 1';
  $limits{'multi_drop'}		= 0; # Drop table can take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can't use group functions in HAVING
  $limits{'like_with_column'}	= 0; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Can use 'GROUP BY 1'
  $limits{'alter_table'}	= 1;
  $limits{'alter_add_multi_col'}= 0;
  $limits{'alter_table_dropcol'}= 0;

  $limits{'group_func_extra_std'} = 0; # Have group function std().

  $limits{'func_odbc_mod'}	= 1; # Have function mod.
  $limits{'func_extra_%'}	= 0; # Has % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 0; # Has function in
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 
  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  return "IBM DB2 5";		#DBI/ODBC can't return the server version
}

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user, $main::opt_password) ||
    die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index) = @_;
  my($query,@queries,$nr);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/mediumint/integer/i;
    $field =~ s/float\(\d+,\d+\)/float/i;
    $field =~ s/integer\(\d+\)/integer/i;
    $field =~ s/int\(\d+\)/integer/i;
    $field =~ s/tinyint\(\d+\)/smallint/i;
    $field =~ s/tinyint/smallint/i;
    $field =~ s/smallint\(\d+\)/smallint/i;
    $field =~ s/smallinteger/smallint/i;
    $field =~ s/blob/varchar(256)/i;
    $query.= $field . ',';
  }
  substr($query,-1)=")";		# Remove last ',';
  push(@queries,$query);
  $nr=0;
  foreach $index (@$index)
  {
    $ext="WITH DISALLOW NULL";
    if (($index =~ s/primary key/unique index primary_key/i))
    {
      $ext="WITH PRIMARY;"
    }
    if ($index =~ /^unique.*\(([^\(]*)\)$/i)
    {
      $nr++;
      $index="unique index ${table_name}_$nr ($1)";
    }
    $index =~ /^(.*)\s+(\(.*\))$/;
    push(@queries,"create ${1} on $table_name $2");
  }
  return @queries;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index
{
  my ($self,$table,$index) = @_;
  return "DROP INDEX $table.$index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 1;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for MIMER 
#############################################################################

package db_Mimer;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "mimer";
  $self->{'data_source'}	= "DBI:mimer:$database:$host";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "binary varying(15000)";
  $self->{'text'}		= "character varying(15000)";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{'char_null'}          = "cast(NULL as char(1))";
  $self->{'numeric_null'}       = "cast(NULL as int)";

  $limits{'max_conditions'}	= 9999; # (Actually not a limit)
  $limits{'max_columns'}	= 252;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 15000; # Max size with default buffers.
  $limits{'query_size'}		= 1000000; # Max size with default buffers.
  $limits{'max_index'}		= 32; # Max number of keys
  $limits{'max_index_parts'}	= 16; # Max segments/key
  $limits{'max_column_name'}	= 128; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 1; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'} = 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0; # Cannot do 'select 1';
  $limits{'multi_drop'}		= 0; # Drop table cannot take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Cannot use 'GROUP BY 1'
  $limits{'alter_table'}	= 1; # Have ALTER TABLE
  $limits{'alter_add_multi_col'}= 0; # Have ALTER TABLE t add a int,add b int;
  $limits{'alter_table_dropcol'}= 1; # Have ALTER TABLE DROP column
  $limits{'insert_multi_value'} = 0; # Does not have INSERT ... values (1,2),(3,4)
  $limits{'multi_distinct'}     = 0; # Does not allow select count(distinct a),count(distinct b).. 

  $limits{'group_func_extra_std'} = 0; # Does not have group function std().

  $limits{'func_odbc_mod'}	= 1; # Have function mod.
  $limits{'func_extra_%'}	= 0; # Does not have % as alias for mod()
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Does not have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 1; # Has function in
  $limits{'limit'}		= 0; # Does not support the limit attribute
  $limits{'unique_index'}	= 1; # Unique index works or not
  $limits{'insert_select'}	= 1;
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 0;
  $limits{'working_all_fields'} = 1;

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
#
#  Pick up SQLGetInfo option SQL_DBMS_VER (18)
#
  $version = $dbh->func(18, GetInfo);
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);  
  return $version;
}

#
# Connection with optional disabling of logging
#

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";

  $dbh->do("SET OPTION LOG_OFF=1,UPDATE_LOG=0");
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index,$options) = @_;
  my($query,@queries,@indexes);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
#    $field =~ s/ decimal/ double(10,2)/i;
#    $field =~ s/ big_decimal/ double(10,2)/i;
    $field =~ s/ double/ double precision/i;  
    $field =~ s/ tinyint\(.*\)/ smallint/i;
    $field =~ s/ smallint\(.*\)/ smallint/i;
    $field =~ s/ mediumint/ integer/i;
    $field =~ s/ float\(.*\)/ float/i;
#    $field =~ s/ date/ int/i;		# Because of tcp ?
    $query.= $field . ',';
  }
  foreach $index (@$index)
  {
    if ( $index =~ /\bINDEX\b/i )
    {
      my @fields = split(' ',$index);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@indexes,$query);
    
    } else {
      $query.= $index . ',';
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  $query.=" $options" if (defined($options));
  push(@queries,$query,@indexes);
  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert of an ascii file isn't supported by Mimer\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index {
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 1 if ($DBI::errstr =~ /Table locked by another cursor/);
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for InterBase
#############################################################################

package db_interbase;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "interbase";
  $self->{'data_source'}	= "DBI:InterBase:database=$database;ib_dialect=3";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "blob";
  $self->{'text'}		= "";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{'char_null'}          = "";
  $self->{'numeric_null'}       = "";

  $limits{'max_conditions'}	= 9999; # (Actually not a limit)
  $limits{'max_columns'}	= 252;	# Max number of columns in table
  $limits{'max_tables'}		= 65000;	# Should be big enough
  $limits{'max_text_size'}	= 15000; # Max size with default buffers.
  $limits{'query_size'}		= 1000000; # Max size with default buffers.
  $limits{'max_index'}		= 65000; # Max number of keys
  $limits{'max_index_parts'}	= 8; # Max segments/key
  $limits{'max_column_name'}	= 128; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 0; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_func_sql_min_str'} = 1; # Can execute MIN() and MAX() on strings
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)
  $limits{'select_without_from'}= 0; # Cannot do 'select 1';
  $limits{'multi_drop'}		= 0; # Drop table cannot take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 1; # Can use group functions in HAVING
  $limits{'like_with_column'}	= 0; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Cannot use 'GROUP BY 1'
  $limits{'alter_table'}	= 1; # Have ALTER TABLE
  $limits{'alter_add_multi_col'}= 1; # Have ALTER TABLE t add a int,add b int;
  $limits{'alter_table_dropcol'}= 1; # Have ALTER TABLE DROP column
  $limits{'insert_multi_value'} = 0; # Does not have INSERT ... values (1,2),(3,4)

  $limits{'group_func_extra_std'} = 0; # Does not have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod.
  $limits{'func_extra_%'}	= 0; # Does not have % as alias for mod()
  $limits{'func_odbc_floor'}	= 0; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Does not have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 0; # Supports -id
  $limits{'func_extra_in_num'}	= 0; # Has function in
  $limits{'limit'}		= 0; # Does not support the limit attribute
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works
  $limits{'order_by_unused'}	= 1;
  $limits{'working_all_fields'} = 1;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$version);
  
  $version='Interbase ?';
  
  $dbh=$self->connect();
  eval { $version =   $dbh->func('version','ib_database_info')->{'version'}; }; 
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

#
# Connection with optional disabling of logging
#

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0, AutoCommit => 1}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";

  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index,$options) = @_;
  my($query,@queries,@keys,@indexes);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
#    $field =~ s/ big_decimal/ decimal/i;
    $field =~ s/ double/ double precision/i;
    $field =~ s/ tinyint/ smallint/i;
    $field =~ s/ mediumint/ integer/i;
    $field =~ s/\bint\b/integer/i;
    $field =~ s/ float\(\d,\d\)/ float/i;
    $field =~ s/ smallint\(\d\)/ smallint/i;
    $field =~ s/ integer\(\d\)/ integer/i;
    $query.= $field . ',';
  }
  foreach $ind (@$index)
  {
    if ( $ind =~ /(\bKEY\b)|(\bUNIQUE\b)/i ){
      push(@keys,"ALTER TABLE $table_name ADD $ind");
    }else{
      my @fields = split(' ',$ind);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@indexes,$query);
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  $query.=" $options" if (defined($options));
  push(@queries,$query,@keys,@indexes);
  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert of an ascii file isn't supported by InterBase\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index {
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 1 if ($DBI::errstr =~ /Table locked by another cursor/);
  return 0;
}

sub small_rollback_segment
{
  return 1;
}

sub reconnect_on_errors
{
  return 1;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for FrontBase 
#############################################################################

package db_FrontBase;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "FrontBase";
  $self->{'data_source'}	= "DBI:FB:dbname=$database;host=$host";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "varchar(8000000)";
  $self->{'text'}		= "varchar(8000000)";
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= ' restrict';
  $self->{'transactions'}	= 1; # Transactions enabled
  $self->{'error_on_execute_means_zero_rows'}=1;

  $limits{'max_conditions'}	= 5427; # (Actually not a limit)
  # The following should be 8192, but is smaller because Frontbase crashes..
  $limits{'max_columns'}	= 150;	# Max number of columns in table
  $limits{'max_tables'}		= 5000;	# 10000 crashed FrontBase
  $limits{'max_text_size'}	= 65000; # Max size with default buffers.
  $limits{'query_size'}		= 8000000; # Max size with default buffers.
  $limits{'max_index'}		= 38; # Max number of keys
  $limits{'max_index_parts'}	= 20; # Max segments/key
  $limits{'max_column_name'}	= 128; # max table and column name

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables
  $limits{'load_data_infile'}	= 1; # Has load data infile
  $limits{'lock_tables'}	= 0; # Has lock tables
  $limits{'functions'}		= 1; # Has simple functions (+/-)
  $limits{'group_functions'}	= 1; # Have group functions
  $limits{'group_distinct_functions'}= 0; # Have count(distinct)
  $limits{'select_without_from'}= 0;
  $limits{'multi_drop'}		= 0; # Drop table cannot take many tables
  $limits{'subqueries'}		= 1; # Supports sub-queries.
  $limits{'left_outer_join'}	= 1; # Supports left outer joins
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING
  $limits{'having_with_group'}	= 0; # Can use group functions in HAVING
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1'
  $limits{'group_by_position'}  = 0; # Use of 'GROUP BY 1'
  $limits{'alter_table'}	= 1; # Have ALTER TABLE
  $limits{'alter_add_multi_col'}= 0; # Have ALTER TABLE t add a int,add b int;
  $limits{'alter_table_dropcol'}= 0; # Have ALTER TABLE DROP column
  $limits{'insert_multi_value'} = 1;

  $limits{'group_func_extra_std'} = 0; # Does not have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod.
  $limits{'func_extra_%'}	= 0; # Does not have % as alias for mod()
  $limits{'func_odbc_floor'}	= 0; # Has func_odbc_floor function
  $limits{'func_extra_if'}	= 0; # Does not have function if.
  $limits{'column_alias'}	= 1; # Alias for fields in select statement.
  $limits{'NEG'}		= 1; # Supports -id
  $limits{'func_extra_in_num'}	= 0; # Has function in
  $limits{'limit'}		= 0; # Does not support the limit attribute
  $limits{'insert_select'}	= 0;
  $limits{'order_by_unused'}	= 0;

  # We don't get an error for duplicate row in 'test-insert'
  $limits{'unique_index'}	= 0; # Unique index works or not
  # We can't use a blob as a normal string (we got a wierd error)
  $limits{'working_blobs'}	= 0;
  # 'select min(region),max(region) from bench1' kills the server after a while
  $limits{'group_func_sql_min_str'} = 0;
  # If you do select f1,f2,f3...f200 from table, Frontbase dies.
  $limits{'working_all_fields'} = 0;
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 

  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

#  $dbh=$self->connect();
#
#  Pick up SQLGetInfo option SQL_DBMS_VER (18)
#
  #$version = $dbh->func(18, GetInfo);
  $version="FrontBase 3.3";
#  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

#
# Connection with optional disabling of logging
#

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, 
		    $main::opt_user,
		    $main::opt_password,
		    { PrintError => 0 , 
		      'fb_host'=>$main::opt_host
		    }) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";
  $db->{AutoCommit}=1;
  # $dbh->do("SET OPTION LOG_OFF=1,UPDATE_LOG=0");
  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#
# If one uses $main::opt_fast then one is allowed to use
# non standard types to get better speed.
#

sub create
{
  my($self,$table_name,$fields,$index,$options) = @_;
  my($query,@queries,@indexes,@keys);

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/ blob/ varchar(32000)/i;
    $field =~ s/ big_decimal/ float/i;
    $field =~ s/ double/ float/i;
    $field =~ s/ tinyint/ smallint/i;
    $field =~ s/ mediumint/ int/i;
    $field =~ s/ integer/ int/i;
    $field =~ s/ float\(\d,\d\)/ float/i;
    $field =~ s/ smallint\(\d\)/ smallint/i;
    $field =~ s/ int\(\d\)/ int/i;
    $query.= $field . ',';
  }
  foreach $ind (@$index)
  {
#    my @index;
    if ( $ind =~ /(\bKEY\b)|(\bUNIQUE\b)/i ){
      push(@keys,"ALTER TABLE $table_name ADD $ind");
    }else{
      my @fields = split(' ',$ind);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@indexes,$query);
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  $query.=" $options" if (defined($options));
  push(@queries,$query,@keys,@indexes);
  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert of an ascii file isn't supported by InterBase\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index {
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0 if ($DBI::errstr =~ /No raw data handle/);
  return 1;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 1;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

#############################################################################
#	     Configuration for SAPDB 
#############################################################################

package db_sapdb;

sub new
{
  my ($type,$host,$database)= @_;
  my $self= {};
  my %limits;
  bless $self;

  $self->{'cmp_name'}		= "sapdb";
  $self->{'data_source'}	= "DBI:SAP_DB:$database";
  $self->{'limits'}		= \%limits;
  $self->{'blob'}		= "LONG"; # *
  $self->{'text'}		= "LONG"; # *
  $self->{'double_quotes'}	= 1; # Can handle:  'Walker''s'
  $self->{'drop_attr'}		= "";
  $self->{'transactions'}	= 1; # Transactions enabled *
  $self->{'char_null'}          = "";
  $self->{'numeric_null'}       = "";

  $limits{'max_conditions'}	= 9999; # (Actually not a limit) *
  $limits{'max_columns'}	= 1023;	# Max number of columns in table *
  $limits{'max_tables'}		= 65000;	# Should be big enough * unlimited actually
  $limits{'max_text_size'}	= 15000; # Max size with default buffers. 
  $limits{'query_size'}		= 64*1024; # Max size with default buffers. *64 kb by default. May be set by system variable 
  $limits{'max_index'}		= 510; # Max number of keys *
  $limits{'max_index_parts'}	= 16; # Max segments/key *
  $limits{'max_column_name'}	= 32; # max table and column name * 

  $limits{'join_optimizer'}	= 1; # Can optimize FROM tables *
  $limits{'load_data_infile'}	= 0; # Has load data infile *
  $limits{'lock_tables'}	= 1; # Has lock tables 
  $limits{'functions'}		= 1; # Has simple functions (+/-) *
  $limits{'group_functions'}	= 1; # Have group functions *
  $limits{'group_func_sql_min_str'} = 1; # Can execute MIN() and MAX() on strings *
  $limits{'group_distinct_functions'}= 1; # Have count(distinct)  *
  $limits{'select_without_from'}= 0; # Cannot do 'select 1';  *
  $limits{'multi_drop'}		= 0; # Drop table cannot take many tables *
  $limits{'subqueries'}		= 1; # Supports sub-queries. *
  $limits{'left_outer_join'}	= 1; # Supports left outer joins *
  $limits{'table_wildcard'}	= 1; # Has SELECT table_name.*
  $limits{'having_with_alias'}  = 0; # Can use aliases in HAVING *
  $limits{'having_with_group'}	= 1; # Can use group functions in HAVING *
  $limits{'like_with_column'}	= 1; # Can use column1 LIKE column2 *
  $limits{'order_by_position'}  = 1; # Can use 'ORDER BY 1' *
  $limits{'group_by_position'}  = 0; # Cannot use 'GROUP BY 1' *
  $limits{'alter_table'}	= 1; # Have ALTER TABLE *
  $limits{'alter_add_multi_col'}= 1; # Have ALTER TABLE t add a int,add b int; *
  $limits{'alter_table_dropcol'}= 1; # Have ALTER TABLE DROP column  *
  $limits{'insert_multi_value'} = 0; # INSERT ... values (1,2),(3,4) *

  $limits{'group_func_extra_std'} = 0; # Does not have group function std().

  $limits{'func_odbc_mod'}	= 0; # Have function mod. *
  $limits{'func_extra_%'}	= 0; # Does not have % as alias for mod() *
  $limits{'func_odbc_floor'}	= 1; # Has func_odbc_floor function *
  $limits{'func_extra_if'}	= 0; # Does not have function if. *
  $limits{'column_alias'}	= 1; # Alias for fields in select statement. *
  $limits{'NEG'}		= 1; # Supports -id *
  $limits{'func_extra_in_num'}	= 0; # Has function in *
  $limits{'limit'}		= 0; # Does not support the limit attribute *
  $limits{'working_blobs'}	= 1; # If big varchar/blobs works *
  $limits{'order_by_unused'}	= 1; # 
  $limits{'working_all_fields'} = 1; #
  $limits{'multi_distinct'}     = 1; # allows select count(distinct a),count(distinct b).. 


  return $self;
}

#
# Get the version number of the database
#

sub version
{
  my ($self)=@_;
  my ($dbh,$sth,$version,@row);

  $dbh=$self->connect();
  $sth = $dbh->prepare("SELECT KERNEL FROM VERSIONS") or die $DBI::errstr;
  $version="SAP DB (unknown)";
  if ($sth->execute && (@row = $sth->fetchrow_array)
      && $row[0] =~ /([\d\.]+)/)
  {
    $version=$row[0];
    $version =~ s/KERNEL/SAP DB/i; 
  }
  $sth->finish;
  $dbh->disconnect;
  $version .= "/ODBC" if ($self->{'data_source'} =~ /:ODBC:/);
  return $version;
}

#
# Connection with optional disabling of logging
#

sub connect
{
  my ($self)=@_;
  my ($dbh);
  $dbh=DBI->connect($self->{'data_source'}, $main::opt_user,
		    $main::opt_password,{ PrintError => 0, AutoCommit => 1}) ||
		      die "Got error: '$DBI::errstr' when connecting to " . $self->{'data_source'} ." with user: '$main::opt_user' password: '$main::opt_password'\n";

  return $dbh;
}

#
# Returns a list of statements to create a table
# The field types are in ANSI SQL format.
#

sub create
{
  my($self,$table_name,$fields,$index,$options) = @_;
  my($query,@queries,$nr);
  my @index;
  my @keys;

  $query="create table $table_name (";
  foreach $field (@$fields)
  {
    $field =~ s/\bmediumint\b/int/i;
    $field =~ s/\btinyint\b/int/i;
    $field =~ s/ int\(\d\)/ int/i;
    $field =~ s/BLOB/LONG/i;
    $field =~ s/INTEGER\s*\(\d+\)/INTEGER/i;
    $field =~ s/SMALLINT\s*\(\d+\)/SMALLINT/i;
    $field =~ s/FLOAT\s*\((\d+),\d+\)/FLOAT\($1\)/i;
    $field =~ s/DOUBLE/FLOAT\(38\)/i;
    $field =~ s/DOUBLE\s+PRECISION/FLOAT\(38\)/i;
    $query.= $field . ',';
  }
  $nr=0;
  foreach $ind (@$index)
  {
    if ( $ind =~ /\bKEY\b/i ){
      push(@keys,"ALTER TABLE $table_name ADD $ind");
    } elsif ($ind =~ /^unique.*\(([^\(]*)\)$/i)  {
      $nr++;
      my $query="create unique index ${table_name}_$nr on $table_name ($1)";
      push(@index,$query);
    }else{
      my @fields = split(' ',$ind);
      my $query="CREATE INDEX $fields[1] ON $table_name $fields[2]";
      push(@index,$query);
    }
  }
  substr($query,-1)=")";		# Remove last ',';
  $query.=" $options" if (defined($options));
  push(@queries,$query);
  push(@queries,@keys);
  push(@queries,@index);
  return @queries;
}

sub insert_file {
  my($self,$dbname, $file) = @_;
  print "insert of an ascii file isn't supported by SAPDB\n";
  return 0;
}

#
# Do any conversions to the ANSI SQL query so that the database can handle it
#

sub query {
  my($self,$sql) = @_;
  return $sql;
}

sub drop_index {
  my ($self,$table,$index) = @_;
  return "DROP INDEX $index";
}

#
# Abort if the server has crashed
# return: 0 if ok
#	  1 question should be retried
#

sub abort_if_fatal_error
{
  return 0;
}

sub small_rollback_segment
{
  return 0;
}

sub reconnect_on_errors
{
  return 0;
}

sub fix_for_insert
{
  my ($self,$cmd) = @_;
  return $cmd;
}

1;
