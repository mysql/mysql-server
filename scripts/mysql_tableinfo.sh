#!@PERL@ -w

use strict;
use Getopt::Long;
use DBI;

=head1 NAME

WARNING: MySQL versions 5.0 and above feature the INFORMATION_SCHEMA
pseudo-database which contains always up-to-date metadata information
about all tables. So instead of using this script one can now
simply query the INFORMATION_SCHEMA.SCHEMATA, INFORMATION_SCHEMA.TABLES,
INFORMATION_SCHEMA.COLUMNS, INFORMATION_SCHEMA.STATISTICS pseudo-tables.
Please see the MySQL manual for more information about INFORMATION_SCHEMA.
This script will be removed from the MySQL distribution in version 5.1.

mysql_tableinfo - creates and populates information tables with 
the output of SHOW DATABASES, SHOW TABLES (or SHOW TABLE STATUS), 
SHOW COLUMNS and SHOW INDEX.

This is version 1.1.

=head1 SYNOPSIS
 
  mysql_tableinfo [OPTIONS] database_to_write [database_like_wild] [table_like_wild]

  Do not backquote (``) database_to_write, 
  and do not quote ('') database_like_wild or table_like_wild

  Examples:

  mysql_tableinfo info

  mysql_tableinfo info this_db

  mysql_tableinfo info %a% b%

  mysql_tableinfo info --clear-only

  mysql_tableinfo info --col --idx --table-status

=cut

# Documentation continued at end of file


sub usage {
    die @_,"\nExecute 'perldoc $0' for documentation\n";
}

my %opt = (
    'user'	=> scalar getpwuid($>),
    'host'      => "localhost",
    'prefix'    => "", #to avoid 'use of uninitialized value...'
);
Getopt::Long::Configure(qw(no_ignore_case)); # disambuguate -p and -P
GetOptions( \%opt,
    "help",
    "user|u=s",
    "password|p=s",
    "host|h=s",
    "port|P=s",
    "socket|S=s",
    "tbl-status",
    "col",
    "idx",
    "clear",
    "clear-only",
    "prefix=s",
    "quiet|q",
) or usage("Invalid option");

if (!$opt{'quiet'})
    {
    print <<EOF
WARNING: MySQL versions 5.0 and above feature the INFORMATION_SCHEMA
pseudo-database which contains always up-to-date metadata information
about all tables. So instead of using this script one can now
simply query the INFORMATION_SCHEMA.SCHEMATA, INFORMATION_SCHEMA.TABLES,
INFORMATION_SCHEMA.COLUMNS, INFORMATION_SCHEMA.STATISTICS pseudo-tables.
Please see the MySQL manual for more information about INFORMATION_SCHEMA.
This script will be removed from the MySQL distribution in version 5.1.
EOF
    }

if ($opt{'help'}) {usage();}

my ($db_to_write,$db_like_wild,$tbl_like_wild);
if (@ARGV==0)
{
    usage("Not enough arguments");
}
$db_to_write="`$ARGV[0]`"; shift @ARGV;
$db_like_wild=($ARGV[0])?$ARGV[0]:"%"; shift @ARGV;
$tbl_like_wild=($ARGV[0])?$ARGV[0]:"%"; shift @ARGV;
if (@ARGV>0) { usage("Too many arguments"); }

$0 = $1 if $0 =~ m:/([^/]+)$:;

my $info_db="`".$opt{'prefix'}."db`";
my $info_tbl="`".$opt{'prefix'}."tbl".
    (($opt{'tbl-status'})?"_status":"")."`";
my $info_col="`".$opt{'prefix'}."col`";
my $info_idx="`".$opt{'prefix'}."idx`";


# --- connect to the database ---

my $dsn = ";host=$opt{'host'}";
$dsn .= ";port=$opt{'port'}" if $opt{'port'};
$dsn .= ";mysql_socket=$opt{'socket'}" if $opt{'socket'};

my $dbh = DBI->connect("dbi:mysql:$dsn;mysql_read_default_group=perl",
                        $opt{'user'}, $opt{'password'},
{
    RaiseError => 1,
    PrintError => 0,
    AutoCommit => 1,
});

$db_like_wild=$dbh->quote($db_like_wild);
$tbl_like_wild=$dbh->quote($tbl_like_wild);

#Ask

if (!$opt{'quiet'})
{
    print "\n!! This program is going to do:\n\n";
    print "**DROP** TABLE ...\n" if ($opt{'clear'} or $opt{'clear-only'});
    print "**DELETE** FROM ... WHERE `Database` LIKE $db_like_wild AND `Table` LIKE $tbl_like_wild
**INSERT** INTO ...

on the following tables :\n";

    foreach  (($info_db, $info_tbl),
	      (($opt{'col'})?$info_col:()), 
	      (($opt{'idx'})?$info_idx:()))
    {
	print("  $db_to_write.$_\n");
    }
    print "\nContinue (you can skip this confirmation step with --quiet) ? (y|n) [n]";
    if (<STDIN> !~ /^\s*y\s*$/i) 
    {
	print "Nothing done!\n";exit;
    }
}

if ($opt{'clear'} or $opt{'clear-only'}) 
{
#do not drop the $db_to_write database !
    foreach  (($info_db, $info_tbl),
	      (($opt{'col'})?$info_col:()), 
	      (($opt{'idx'})?$info_idx:()))
    {
	$dbh->do("DROP TABLE IF EXISTS $db_to_write.$_");
    }
    if ($opt{'clear-only'}) 
    {
	print "Wrote to database $db_to_write .\n" unless ($opt{'quiet'});
	exit; 
    }
}


my %sth;
my %extra_col_desc;
my %row;
my %done_create_table;

#create the $db_to_write database
$dbh->do("CREATE DATABASE IF NOT EXISTS $db_to_write");
$dbh->do("USE $db_to_write");

#get databases
$sth{'db'}=$dbh->prepare("SHOW DATABASES LIKE $db_like_wild");
$sth{'db'}->execute;

#create $info_db which will receive info about databases.
#Ensure that the first column to be called "Database" (as SHOW DATABASES LIKE
#returns a varying
#column name (of the form "Database (%...)") which is not suitable)
$extra_col_desc{'db'}=do_create_table("db",$info_db,undef,"`Database`");
#we'll remember the type of the `Database` column (as returned by
#SHOW DATABASES), which we will need when creating the next tables. 

#clear out-of-date info from this table 
$dbh->do("DELETE FROM $info_db WHERE `Database` LIKE $db_like_wild"); 


while ($row{'db'}=$sth{'db'}->fetchrow_arrayref) #go through all databases
{

#insert the database name
    $dbh->do("INSERT INTO $info_db VALUES("
	     .join(',' ,  ( map $dbh->quote($_), @{$row{'db'}} ) ).")" );

#for each database, get tables

    $sth{'tbl'}=$dbh->prepare("SHOW TABLE"
			    .( ($opt{'tbl-status'}) ? 
			       " STATUS"
			       : "S" )
			    ." from `$row{'db'}->[0]` LIKE $tbl_like_wild");
    $sth{'tbl'}->execute;
    unless ($done_create_table{$info_tbl})

#tables must be created only once, and out-of-date info must be
#cleared once
    {
	$done_create_table{$info_tbl}=1;
	$extra_col_desc{'tbl'}=
	    do_create_table("tbl",$info_tbl,
#add an extra column (database name) at the left
#and ensure that the table name will be called "Table"
#(this is unncessesary with
#SHOW TABLE STATUS, but necessary with SHOW TABLES (which returns a column
#named "Tables_in_..."))
			    "`Database` ".$extra_col_desc{'db'},"`Table`"); 
	$dbh->do("DELETE FROM $info_tbl WHERE `Database` LIKE $db_like_wild		                         AND `Table` LIKE $tbl_like_wild");
    }

    while ($row{'tbl'}=$sth{'tbl'}->fetchrow_arrayref)
    {
	$dbh->do("INSERT INTO $info_tbl VALUES("
		 .$dbh->quote($row{'db'}->[0]).","
		 .join(',' ,  ( map $dbh->quote($_), @{$row{'tbl'}} ) ).")");

#for each table, get columns...

	if ($opt{'col'})
	{
	    $sth{'col'}=$dbh->prepare("SHOW COLUMNS FROM `$row{'tbl'}->[0]` FROM `$row{'db'}->[0]`"); 
	    $sth{'col'}->execute;
	    unless ($done_create_table{$info_col})
	    {
		$done_create_table{$info_col}=1;
		do_create_table("col",$info_col,
				"`Database` ".$extra_col_desc{'db'}.","
				."`Table` ".$extra_col_desc{'tbl'}.","
				."`Seq_in_table` BIGINT(3)"); 
#We need to add a sequence number (1 for the first column of the table,
#2 for the second etc) so that users are able to retrieve columns in order
#if they want. This is not needed for INDEX 
#(where there is already Seq_in_index)
		$dbh->do("DELETE FROM $info_col WHERE `Database` 
                            LIKE $db_like_wild
			    AND `Table` LIKE $tbl_like_wild");
	    }
	    my $col_number=0;
	    while ($row{'col'}=$sth{'col'}->fetchrow_arrayref)
	    {
		$dbh->do("INSERT INTO $info_col VALUES("
			 .$dbh->quote($row{'db'}->[0]).","
			 .$dbh->quote($row{'tbl'}->[0]).","
			 .++$col_number.","
			 .join(',' ,  ( map $dbh->quote($_), @{$row{'col'}} ) ).")");
	    }
	}

#and get index.

	if ($opt{'idx'})
	{
	    $sth{'idx'}=$dbh->prepare("SHOW INDEX FROM `$row{'tbl'}->[0]` FROM `$row{'db'}->[0]`"); 
	    $sth{'idx'}->execute;
	    unless ($done_create_table{$info_idx})
	    {
		$done_create_table{$info_idx}=1;
		do_create_table("idx",$info_idx,
				"`Database` ".$extra_col_desc{'db'});
		$dbh->do("DELETE FROM $info_idx WHERE `Database`
			 LIKE $db_like_wild
			 AND `Table` LIKE $tbl_like_wild");
	    }
	    while ($row{'idx'}=$sth{'idx'}->fetchrow_arrayref)
	    {
		$dbh->do("INSERT INTO $info_idx VALUES("
			 .$dbh->quote($row{'db'}->[0]).","
			 .join(',' ,  ( map $dbh->quote($_), @{$row{'idx'}} ) ).")");
	    }
	}
    }
}

print "Wrote to database $db_to_write .\n" unless ($opt{'quiet'});
exit;


sub do_create_table
{
    my ($sth_key,$target_tbl,$extra_col_desc,$first_col_name)=@_; 
    my $create_table_query=$extra_col_desc;
    my ($i,$first_col_desc,$col_desc);

    for ($i=0;$i<$sth{$sth_key}->{NUM_OF_FIELDS};$i++)
    {
	if ($create_table_query) { $create_table_query.=", "; }	
	$col_desc=$sth{$sth_key}->{mysql_type_name}->[$i];
	if ($col_desc =~ /char|int/i)
	{
	    $col_desc.="($sth{$sth_key}->{PRECISION}->[$i])";
	}
	elsif ($col_desc =~ /decimal|numeric/i) #(never seen that)
	{
	    $col_desc.=
		"($sth{$sth_key}->{PRECISION}->[$i],$sth{$sth_key}->{SCALE}->[$i])";
	}
	elsif ($col_desc !~ /date/i) #date and datetime are OK,
	                         #no precision or scale for them
	{
	    warn "unexpected column type '$col_desc' 
(neither 'char','int','decimal|numeric')
when creating $target_tbl, hope table creation will go OK\n";
	}
	if ($i==0) {$first_col_desc=$col_desc};
	$create_table_query.=
	    ( ($i==0 and $first_col_name) ? 
	      "$first_col_name " :"`$sth{$sth_key}->{NAME}->[$i]` " )
	    .$col_desc;
    }
if ($create_table_query)
{
    $dbh->do("CREATE TABLE IF NOT EXISTS $target_tbl ($create_table_query)");
}
return $first_col_desc;
}

__END__


=head1 DESCRIPTION

mysql_tableinfo asks a MySQL server information about its
databases, tables, table columns and index, and stores this
in tables called `db`, `tbl` (or `tbl_status`), `col`, `idx` 
(with an optional prefix specified with --prefix).
After that, you can query these information tables, for example
to build your admin scripts with SQL queries, like

SELECT CONCAT("CHECK TABLE ",`database`,".",`table`," EXTENDED;") 
FROM info.tbl WHERE ... ;

as people usually do with some other RDBMS
(note: to increase the speed of your queries on the info tables,
you may add some index on them).

The database_like_wild and table_like_wild instructs the program
to gather information only about databases and tables
whose names match these patterns. If the info
tables already exist, their rows matching the patterns are simply
deleted and replaced by the new ones. That is,
old rows not matching the patterns are not touched.
If the database_like_wild and table_like_wild arguments
are not specified on the command-line they default to "%".

The program :

- does CREATE DATABASE IF NOT EXISTS database_to_write
where database_to_write is the database name specified on the command-line.

- does CREATE TABLE IF NOT EXISTS database_to_write.`db`

- fills database_to_write.`db` with the output of
SHOW DATABASES LIKE database_like_wild

- does CREATE TABLE IF NOT EXISTS database_to_write.`tbl`
(respectively database_to_write.`tbl_status`
if the --tbl-status option is on)

- for every found database,
fills database_to_write.`tbl` (respectively database_to_write.`tbl_status`)
with the output of 
SHOW TABLES FROM found_db LIKE table_like_wild
(respectively SHOW TABLE STATUS FROM found_db LIKE table_like_wild)

- if the --col option is on,
    * does CREATE TABLE IF NOT EXISTS database_to_write.`col`
    * for every found table,
      fills database_to_write.`col` with the output of 
      SHOW COLUMNS FROM found_tbl FROM found_db

- if the --idx option is on,
    * does CREATE TABLE IF NOT EXISTS database_to_write.`idx`
    * for every found table,
      fills database_to_write.`idx` with the output of 
      SHOW INDEX FROM found_tbl FROM found_db

Some options may modify this general scheme (see below).

As mentioned, the contents of the info tables are the output of
SHOW commands. In fact the contents are slightly more complete :

- the `tbl` (or `tbl_status`) info table 
  has an extra column which contains the database name,

- the `col` info table
  has an extra column which contains the table name,
  and an extra column which contains, for each described column,
  the number of this column in the table owning it (this extra column
  is called `Seq_in_table`). `Seq_in_table` makes it possible for you
  to retrieve your columns in sorted order, when you are querying
  the `col` table. 

- the `index` info table
  has an extra column which contains the database name.

Caution: info tables contain certain columns (e.g.
Database, Table, Null...) whose names, as they are MySQL reserved words,
need to be backquoted (`...`) when used in SQL statements.

Caution: as information fetching and info tables filling happen at the
same time, info tables may contain inaccurate information about
themselves.

=head1 OPTIONS

=over 4

=item --clear

Does DROP TABLE on the info tables (only those that the program is
going to fill, for example if you do not use --col it won't drop
the `col` table) and processes normally. Does not drop database_to_write.

=item --clear-only

Same as --clear but exits after the DROPs.

=item --col

Adds columns information (into table `col`).

=item --idx

Adds index information (into table `idx`).

=item --prefix prefix

The info tables are named from the concatenation of prefix and,
respectively, db, tbl (or tbl_status), col, idx. Do not quote ('')
or backquote (``) prefix.

=item -q, --quiet

Does not warn you about what the script is going to do (DROP TABLE etc)
and does not ask for a confirmation before starting.

=item --tbl-status

Instead of using SHOW TABLES, uses SHOW TABLE STATUS
(much more complete information, but slower). 

=item --help

Display helpscreen and exit

=item -u, --user=#         

user for database login if not current user. Give a user
who has sufficient privileges (CREATE, ...).

=item -p, --password=#     

password to use when connecting to server

=item -h, --host=#     

host to connect to

=item -P, --port=#         

port to use when connecting to server

=item -S, --socket=#         

UNIX domain socket to use when connecting to server

=head1 WARRANTY

This software is free and comes without warranty of any kind.

Patches adding bug fixes, documentation and new features are welcome.

=head1 TO DO

Nothing: starting from MySQL 5.0, this program is replaced by the
INFORMATION_SCHEMA pseudo-database.

=head1 AUTHOR

2002-06-18 Guilhem Bichot (guilhem.bichot@mines-paris.org)

And all the authors of mysqlhotcopy, which served as a model for 
the structure of the program.
