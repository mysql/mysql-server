#!/usr/bin/perl
# Copyright (C) 2000, 2003 MySQL AB
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
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# start initialition
#

$VER = "1.0";

use Getopt::Long;
use Cwd;
use DBI;

$max_row_length=500000;		# Don't create bigger SQL rows that this
$opt_lock=1;			# lock tables

$pwd = cwd(); $pwd = "." if ($pwd eq '');

require "$pwd/server-cfg" || die "Can't read Configuration file: $!\n";

$|=1;

$opt_from_server= $opt_to_server= "mysql";
$opt_from_host= $opt_to_host=     "localhost";
$opt_from_db= $opt_to_db=         "test";
$opt_from_user=$opt_from_password=$opt_to_user=$opt_to_password="";
$opt_help=$opt_verbose=$opt_debug=0;


GetOptions("from-server=s","to-server=s","from-host=s","to-host=s","from-db=s",
	   "to-db=s", "help", "verbose","debug") || usage();

usage() if ($opt_help || 
	    ($opt_from_server eq $opt_to_server && 
	     $opt_from_db eq $opt_to_db &&
	     $opt_from_host eq $opt_to_host));

####
#### Usage
####


sub usage
{
  print <<EOF;

$0 version $VER by Monty

 Copies tables between two database servers. If the destination table doesn\'t
 exist it\'s autoamticly created.  If the destination table exists, it
 should be compatible with the source table.

 Because DBI doesn\'t provide full information about the columns in a table,
 some columns may not have optimal types in a create tables.  Any created
 tables will also not have any keys!

  Usage: $0 [options] tables...

  Options:
  --help         Show this help and exit
  --from-server	  Source server			(Default: $opt_from_server)
  --from-host     Source hostname		(Default: $opt_from_host)
  --from-db       Source database name		(Default: $opt_from_db)
  --from-user	  Source user			(Default: $opt_from_password)
  --from-password Source password		(Default: $opt_from_password)
  --to-server     Destination server		(Default: $opt_to_server)
  --to-host       Destination hostname		(Default: $opt_to_host)
  --to-db         Destination database name	(Default: $opt_to_db)
  --to-user	  Destination user		(Default: $opt_to_user)
  --to-password	  Destination password		(Default: $opt_to_password)
  --verbose	  Be more verbose

  If you the server names ends with _ODBC, then this program will connect
  through ODBC instead of using a native driver.
EOF
   exit(0);
}

####
#### Connect
####

$from_server=get_server($opt_from_server,$opt_from_host,$opt_from_db);
$to_server=get_server($opt_to_server,$opt_to_host,$opt_to_db);

$opt_user=$opt_from_user; $opt_password=$opt_from_password;
print "- connecting to SQL servers\n" if ($opt_verbose);
$from_dbh=$from_server->connect() || die "Can't connect to source server $opt_from_server on host $opt_from_host using db $opt_from_db";
$opt_user=$opt_to_user; $opt_password=$opt_to_password;
$to_dbh=$to_server->connect() || die "Can't connect to source server $opt_to_server on host $opt_to_host using db $opt_to_db";

####
#### Copy data
####

foreach $table (@ARGV)
{

  print "- querying $table\n" if ($opt_verbose);
  $sth=$from_dbh->prepare("select * from $table") || die "Can't prepare query to get $table; $DBI::errstr";
  $sth->execute || die "Can't execute query to get data from $table; $DBI::errstr";

  if (!table_exists($to_server,$to_dbh,$table))
  {
    print "- creating $table\n" if ($opt_verbose);
    $table_def=get_table_definition($from_server,$from_dbh,$sth);
    do_many($to_dbh,$to_server->create($table,$table_def,[]));
  }
  if ($opt_lock && $to_server->{'lock_tables'})
  {
    print "- locking $table\n" if ($opt_verbose);
    $to_dbh->do("lock tables $table WRITE");
  }

  $columns=$sth->{NUM_OF_FIELDS};
  $columns_to_quote=get_columns_to_quote($sth);
  $insert_multi_value=$sth->{'insert_multi_value'};
  $query="insert into $table values"; $result="";

  print "- copying $table\n" if ($opt_verbose);
  while (($row = $sth->fetchrow_arrayref))
  {
    $tmp="(";
    for ($i=0 ; $i < $columns ; $i++)
    {
      if ($columns_to_quote->[$i])
      {
	$tmp.= $to_dbh->quote($row->[$i]) . ",";
      }
      else
      {
	$tmp.= $row->[$i] . ",";	
      }
    }
    substr($tmp,-1)=")";		# Remove last ','
    if ($insert_multi_value)
    {
      $to_dbh->do($query . $tmp) || die "Can't insert row: $DBI::errstr";
    }
    elsif (length($result)+length($tmp) >= $max_row_length && $result)
    {
      $to_dbh->do($query . $result) || die "Can't insert row: $DBI::errstr";
      $result="";
    }
    elsif (length($result))
    {
      $result.= ",$tmp";
    }
    else
    {
      $result=$tmp;
    }
  }
  if (length($result))
  {
    $to_dbh->do($query . $result) || die "Can't insert row: $DBI::errstr";
  }
  if ($opt_lock && $to_server->{'lock_tables'})
  {
    $to_dbh->do("unlock tables");
  }
}


sub get_table_definition
{
  my ($server,$dbh,$sth)=@_;
  my ($i,$names,$types,$scale,$precision,$nullable,@res);

  $names=$sth->{NAME};
  $types=$sth->{TYPE};
  $nullable=$sth->{NULLABLE};
  if (0)
  {
    # The following doesn't yet work
    $scale=$sth->{SCALE};
    $precision=$sth->{PRECISION};
  }
  else
  {
    my (@tmp);
    @tmp= (undef()) x $sth->{NUM_OF_FIELDS};
    $precision= $scale= \@tmp;
  }
  for ($i = 0; $i < $sth->{NUM_OF_FIELDS} ; $i++)
  {
    push(@res,$names->[$i] . " " .
	 odbc_to_sql($server,$types->[$i],$precision->[$i],$scale->[$i]) .
	 ($nullable->[$i] ? "" : " NOT NULL"));
  }
  return \@res;
}


sub odbc_to_sql
{
  my ($server,$type,$precision,$scale)=@_;

  if ($type == DBI::SQL_CHAR())
  {
    return defined($precision) ? "char($precision)" : "varchar(255)";
  }

  if ($type == DBI::SQL_NUMERIC())
  {
    $precision=15 if (!defined($precision));
    $scale=6 if (!defined($scale));
    return "numeric($precision,$scale)";
  }
  if ($type == DBI::SQL_DECIMAL())
  {
    $precision=15 if (!defined($precision));
    $scale=6 if (!defined($scale));
    return "decimal($precision,$scale)";
  }
  if ($type == DBI::SQL_INTEGER())
  {
    return "integer" if (!defined($precision));
    return "integer($precision)";
  }
  if ($type == DBI::SQL_SMALLINT())
  {
    return "smallint" if (!defined($precision));
    return "smallint($precision)";
  }
  if ($type == DBI::SQL_FLOAT())
  {
    $precision=12 if (!defined($precision));
    $scale=2 if (!defined($scale));
    return "float($precision,$scale)";
  }
  if ($type == DBI::SQL_REAL())
  {
    $precision=12 if (!defined($precision));
    $scale=2 if (!defined($scale));
    return "float($precision,$scale)";
  }
  if ($type == DBI::SQL_DOUBLE())
  {
    $precision=22 if (!defined($precision));
    $scale=2 if (!defined($scale));
    return "double($precision,$scale)";
  }
  if ($type == DBI::SQL_VARCHAR())
  {
    $precision=255 if (!defined($precision));
    return "varchar($precision)";
  }
  return "date"				if ($type == DBI::SQL_DATE());
  return "time"				if ($type == DBI::SQL_TIME());
  return "timestamp"			if ($type == DBI::SQL_TIMESTAMP());
  return $server->{'text'}		if ($type == DBI::SQL_LONGVARCHAR());
  return $server->{'blob'}		if ($type == DBI::SQL_LONGVARBINARY());
  if ($type == DBI::SQL_BIGINT())
  {
    return "bigint" if (!defined($precision));
    return "bigint($precision)";
  }
  if ($type == DBI::SQL_TINYINT())
  {
    return "tinyint" if (!defined($precision));
    return "tinyint($precision)";
  }
  die "Can't covert type '$type' to a ODBC type\n";
}

#
# return an array with 1 for all coumns that we have to quote
#
					      
sub get_columns_to_quote($sth)
{
  my ($sth)=@_;
  my ($i,@res,$type,$tmp);

  @res=();
  for ($i = 0; $i < $sth->{NUM_OF_FIELDS} ; $i++)
  {
    $type=$sth->{TYPE}->[$i];
    $tmp=1;			# String by default
    if ($type == DBI::SQL_NUMERIC()	|| $type == DBI::SQL_DECIMAL() ||
	$type == DBI::SQL_INTEGER()	|| $type == DBI::SQL_SMALLINT() ||
	$type == DBI::SQL_SMALLINT()	|| $type == DBI::SQL_FLOAT() ||
	$type == DBI::SQL_REAL() 	|| $type == DBI::SQL_DOUBLE() ||
	$type == DBI::SQL_BIGINT()	|| $type == DBI::SQL_TINYINT())
    {
      $tmp=0;
    }
    push (@res,$tmp);
  }
  return \@res;
}

#
# Check if table exists;  Return 1 if table exists
#

sub table_exists
{
  my ($server,$dbh,$table)=@_;
  if ($server->{'limits'}->{'group_functions'})
  {
    return !safe_query($dbh,"select count(*) from $table");
  }
  if ($server->{'limits'}->{'limit'})
  {
    return !safe_query($dbh,"select * from $table limit 1");
  }
  die "Don't know how to check if table '$table' exists in destination server\n";
}


#
# execute query;  return 0 if query is ok
#

sub safe_query
{
  my ($dbh,$query)=@_;
  my ($sth);

  print "query: $query\n" if ($opt_debug);
  if (!($sth= $dbh->prepare($query)))
  {
    print "error: $DBI::errstr\n" if ($opt_debug);
    return 1;
  }
  if (!$sth->execute)
  {
    print "error: $DBI::errstr\n" if ($opt_debug);
    return 1
  }
  while ($sth->fetchrow_arrayref)
  {
  }
  $sth->finish;
  undef($sth);
  return 0;
}

#
# execute an array of queries
#

sub do_many
{
  my ($dbh,@statements)=@_;
  my ($statement,$sth);

  foreach $statement (@statements)
  {
    print "query: $statement\n" if ($opt_debug);
    if (!($sth=$dbh->do($statement)))
    {
      die "Can't execute command '$statement'\nError: $DBI::errstr\n";
    }
  }
}
