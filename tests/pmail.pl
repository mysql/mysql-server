#!/usr/bin/perl
#                                  
# Prints mails to standard output  
#                                  
####
#### Standard inits and get options
####

use DBI;
use Getopt::Long;

$VER="1.4a";

@fldnms= ("mail_from","mail_to","cc","date","time_zone","file","sbj","txt");
$fields=8;
@mail= (@from,@to,@cc,@date,@time_zone,@file,@sbj,@txt);

$opt_user= $opt_password= "";
$opt_socket= "/tmp/mysql.sock";
$opt_port= 3306;
$opt_db="test";
$opt_table="mails";
$opt_help=$opt_count=0;

GetOptions("help","count","port=i","db=s","table=s","host=s","password=s",
	   "user=s","socket=s") || usage();

if ($opt_host eq '')
{
  $opt_host = "localhost";
}

if ($opt_help || !$ARGV[0])
{
  usage();
}

####
#### Connect and parsing the query to MySQL
####

$dbh= DBI->connect("DBI:mysql:$opt_db:$opt_host:port=$opt_port:mysql_socket=$opt_mysql_socket", $opt_user,$opt_password, { PrintError => 0})
|| die $DBI::errstr;

if ($opt_count)
{
  count_mails();
}

$fields=0;
$query = "select ";
foreach $val (@fldnms)
{
  if (!$fields)
  {
    $query.= "$val";
  }
  else
  {
    $query.= ",$val";
  }
  $fields++;
}
$query.= " from $opt_table where $ARGV[0]";

####
#### Send query and save result
####

$sth= $dbh->prepare($query);
if (!$sth->execute)
{
  print "$DBI::errstr\n";
  $sth->finish;
  die;
}
for ($i=0; ($row= $sth->fetchrow_arrayref); $i++)
{
  for ($j=0; $j < $fields; $j++)
  {
    $mail[$j][$i]= $row->[$j];
  }
}

####
#### Print to stderr
####

for ($i=0; $mail[0][$i]; $i++)
{
  print "#" x 33;
  print " " . ($i+1) . ". Mail ";
  print "#" x 33;
  print "\nFrom: $mail[0][$i]\n";
  print "To: $mail[1][$i]\n";
  print "Cc: $mail[2][$i]\n";
  print "Date: $mail[3][$i]\n";
  print "Timezone: $mail[4][$i]\n";
  print "File: $mail[5][$i]\n";
  print "Subject: $mail[6][$i]\n";
  print "Message:\n$mail[7][$i]\n";
}
print "#" x 20;
print " Summary: ";
if ($i == 1) 
{
  print "$i Mail ";
  print "matches the query ";
}
else
{
  print "$i Mails ";
  print "match the query ";
}
print "#" x 20;
print "\n";

####
#### Count mails that matches the query, but don't show them
####

sub count_mails
{
  $sth= $dbh->prepare("select count(*) from $opt_table where $ARGV[0]");
  if (!$sth->execute)
  {
    print "$DBI::errstr\n";
    $sth->finish;
    die;
  }
  while (($row= $sth->fetchrow_arrayref))
  {
    $mail_count= $row->[0];
  }
  if ($mail_count == 1)
  {  
    print "$mail_count Mail matches the query.\n";
  }
  else
  {
    print "$mail_count Mails match the query.\n";
  }
  exit;
}

####
#### Usage
####

sub usage
{
  print <<EOF;
  pmail version $VER by Jani Tolonen

  Usage: pmail [options] "SQL where clause"
  Options:
  --help      show this help
  --count     Shows how many mails matches the query, but not the mails.
  --db=       database to use (Default: $opt_db)
  --table=    table to use    (Default: $opt_table)
  --host=     Hostname which to connect (Default: $opt_host)
  --socket=   Unix socket to be used for connection (Default: $opt_socket)
  --password= Password to use for mysql
  --user=     User to be used for mysql connection, if not current user
  --port=     mysql port to be used (Default: $opt_port)
  "SQL where clause" is the end of the select clause,
  where the condition is expressed. The result will
  be the mail(s) that matches the condition and
  will be displayed with the fields:
  - From
  - To
  - Cc
  - Date
  - Timezone
  - File (Where from the current mail was loaded into the database)
  - Subject
  - Message text
  The field names that can be used in the where clause are:
    Field      Type 
  - mail_from  varchar(120)
  - date       datetime
  - sbj        varchar(200)
  - txt        mediumtext
  - cc         text
  - mail_to    text
  - time_zone  varchar(6)
  - reply      varchar(120)
  - file       varchar(32)
  - hash       int(11)
  An example of the pmail:
  pmail "txt like '%libmysql.dll%' and sbj like '%delphi%'"
  NOTE: the txt field is NOT case sensitive!
EOF
  exit(0);
}
