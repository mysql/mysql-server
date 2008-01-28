#!/usr/bin/perl
# Copyright (C) 2000-2002 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Convert given tables in a database to MYISAM

use DBI;
use Getopt::Long;

$opt_help=$opt_version=$opt_verbose=$opt_force=0;
$opt_user=$opt_database=$opt_password=undef;
$opt_host="localhost";
$opt_socket="";
$opt_type="MYISAM";
$opt_port=0;
$exit_status=0;

GetOptions("force","help","host=s","password=s","user=s","type=s","verbose","version","socket=s", "port=i") || 
  usage(0);
usage($opt_version) if ($#ARGV < 0 || $opt_help || $opt_version);
$opt_database=shift(@ARGV);

if (uc($opt_type) eq "HEAP")
{
  print "Converting to type HEAP would delete your tables; aborting\n";
  exit(1);
}

$connect_opt="";
if ($opt_port)
{
  $connect_opt.= ";port=$opt_port";
}
if (length($opt_socket))
{
  $connect_opt.=";mysql_socket=$opt_socket";
}

$dbh = DBI->connect("DBI:mysql:$opt_database:${opt_host}$connect_opt",
		    $opt_user,
		    $opt_password,
		    { PrintError => 0})
  || die "Can't connect to database $opt_database: $DBI::errstr\n";

if ($#ARGV < 0)
{
  # Fetch all table names from the database
  my ($sth,$row);
  $sth=$dbh->prepare("show tables");
  $sth->execute || die "Can't get tables from $opt_database; $DBI::errstr\n";
  while (($row = $sth->fetchrow_arrayref))
  {
    push(@ARGV,$row->[0]);
  }
  $sth->finish;
}

print "Converting tables:\n" if ($opt_verbose);
foreach $table (@ARGV)
{
  my ($sth,$row);

  # Check if table is already converted
  $sth=$dbh->prepare("show table status like '$table'");  
  if ($sth->execute && ($row = $sth->fetchrow_arrayref))
  {
    if (uc($row->[1]) eq uc($opt_type))
    {
      print "$table is already of type $opt_type;  Ignored\n";
      next;
    }
  }
  print "converting $table\n" if ($opt_verbose);
  if (!$dbh->do("ALTER TABLE $table ENGINE=$opt_type"))
  {
    print STDERR "Can't convert $table: Error $DBI::errstr\n";
    exit(1) if (!$opt_force);
    $exit_status=1;
  }
}

$dbh->disconnect;
exit($exit_status);


sub usage
{
  my($version)=shift;
  print "$0  version 1.1\n";
  exit(0) if ($version);

  print <<EOF;

Conversion of a MySQL tables to other table types.

 Usage: $0 database [tables]
 If no tables has been specifed, all tables in the database will be converted.

 The following options are available:

--force
  Continue even if there is some error.

--help or --Information
  Shows this help

--host='host name' (Default $opt_host)
  Host name where the database server is located.

--password='password'
  Password for the current user.

--port=port
  TCP/IP port to connect to if host is not "localhost".

--socket='/path/to/socket'
  Socket to connect with.

--ENGINE='table-type'
  Converts tables to the given table type (Default: $opt_type)
  MySQL 3.23 supports at least the BDB, ISAM and MYISAM types.

--user='user_name'
  User name to log into the SQL server.

--verbose
  This is a test specific option that is only used when debugging a test.
  Print more information about what is going on.

--version
  Shows the version of this program.
EOF
  exit(1);
}
