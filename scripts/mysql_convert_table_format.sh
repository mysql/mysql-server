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
$opt_engine="MYISAM";
$opt_port=0;
$exit_status=0;

GetOptions(
  "e|engine|type=s"       => \$opt_type,
  "f|force"               => \$opt_force,
  "help|?"               => \$opt_help,
  "h|host=s"              => \$opt_host,
  "p|password=s"          => \$opt_password,
  "u|user=s"              => \$opt_user,
  "v|verbose"             => \$opt_verbose,
  "V|version"             => \$opt_version,
  "S|socket=s"            => \$opt_socket, 
  "P|port=i"              => \$opt_port
) || usage(0);

usage($opt_version) if ($#ARGV < 0 || $opt_help || $opt_version);

$opt_database=shift(@ARGV);

if (grep { /^$opt_engine$/i } qw(HEAP MEMORY BLACKHOLE))
{
  print "Converting to '$opt_engine' would delete your data; aborting\n";
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

my @tables;

push(@ARGV, "%") if(!@ARGV);

foreach $pattern (@ARGV)
{
  my ($sth,$row);
  $sth=$dbh->prepare("SHOW TABLES LIKE ?");
  $rv= $sth->execute($pattern);
  if(!int($rv))
  {
    warn "Can't get tables matching '$pattern' from $opt_database; $DBI::errstr\n"; 
    exit(1) unless $opt_force;
  }
  while (($row = $sth->fetchrow_arrayref))
  {
    push(@tables, $row->[0]);
  }
  $sth->finish;
}

print "Converting tables:\n" if ($opt_verbose);
foreach $table (@tables)
{
  my ($sth,$row);

  # Check if table is already converted
  $sth=$dbh->prepare("show table status like '$table'");  
  if ($sth->execute && ($row = $sth->fetchrow_arrayref))
  {
    if (uc($row->[1]) eq uc($opt_engine))
    {
      print "$table already uses the '$opt_engine' engine;  Ignored\n";
      next;
    }
  }
  print "converting $table\n" if ($opt_verbose);
  $table=~ s/`/``/g;
  if (!$dbh->do("ALTER TABLE `$table` ENGINE=$opt_engine"))
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

Conversion of a MySQL tables to other storage engines

 Usage: $0 database [table[ table ...]]
 If no tables has been specifed, all tables in the database will be converted.
 You can also use wildcards, ie "my%"

 The following options are available:

-f, --force
  Continue even if there is some error.

-?, --help
  Shows this help

-e, --engine=ENGINE
  Converts tables to the given storage engine (Default: $opt_engine)

-h, --host=HOST
  Host name where the database server is located. (Default: $opt_host)

-p, --password=PASSWORD
  Password for the current user.

-P, --port=PORT
  TCP/IP port to connect to if host is not "localhost".

-S, --socket=SOCKET
  Socket to connect with.

-u, --user=USER
  User name to log into the SQL server.

-v, --verbose
  This is a test specific option that is only used when debugging a test.
  Print more information about what is going on.

-V, --version
  Shows the version of this program.
EOF
  exit(1);
}
