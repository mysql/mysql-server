#!/usr/bin/perl -w

# Copyright (C) 2000, 2005 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#                                  
# Prints mails to standard output  
#                                  
####
#### Standard inits and get options
####

use DBI;
use Getopt::Long;

$VER="2.0";

@fldnms= ("mail_from","mail_to","cc","date","time_zone","file","sbj","txt");
my $fields= 0;
my $base_q= "";
my $mail_count= 0;

$opt_user= $opt_password= "";
$opt_socket= "/tmp/mysql.sock";
$opt_port= 3306;
$opt_db="mail";
$opt_table="my_mail";
$opt_help=$opt_count=0;
$opt_thread= 0;
$opt_host= "";
$opt_message_id= 0;

GetOptions("help","count","port=i","db=s","table=s","host=s","password=s",
	   "user=s","socket=s", "thread","message_id") || usage();

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

$dbh= DBI->connect("DBI:mysql:$opt_db:$opt_host:port=$opt_port:mysql_socket=$opt_socket", $opt_user,$opt_password, { PrintError => 0})
|| die $DBI::errstr;

main();

####
#### main
####

sub main
{
  my ($row, $val, $q, $mail, $sth);

  if ($opt_count)
  {
    count_mails();
  }

  $base_q= "SELECT ";
  foreach $val (@fldnms)
  {
    if (!$fields)
    {
      $base_q.= "$val";
    }
    else
    {
      $base_q.= ",$val";
    }
    $fields++;
  }
  $base_q.= ",message_id" if ($opt_thread || $opt_message_id);
  $base_q.= " FROM $opt_table";
  $q= " WHERE $ARGV[0]";

  $sth= $dbh->prepare($base_q . $q);
  if (!$sth->execute)
  {
    print "$DBI::errstr\n";
    $sth->finish;
    die;
  }
  for (; ($row= $sth->fetchrow_arrayref); $mail_count++)
  {
    for ($i= 0; $i < $fields; $i++)
    {
      if ($opt_message_id)
      {
	$mail[$fields][$mail_count]= $row->[$fields];
	$mail[$fields][$mail_count].= "\nNumber of Replies: " . get_nr_replies($row->[$fields]);
      }
      $mail[$i][$mail_count]= $row->[$i];
    }
    if ($opt_thread)
    {
      get_mail_by_message_id($row->[$fields], $mail);
    }
  }
  print_mails($mail);
}

####
#### Function, which fetches mail by searching in-reply-to with
#### a given message_id. Saves the value (mail) in mail variable.
#### Returns the message id of the mail found and searches again
#### and saves, until no more mails are found with that message_id.
####

sub get_mail_by_message_id
{
  my ($message_id, $mail)= @_;
  my ($q, $query, $i, $row, $sth);

  $q= " WHERE in_reply_to = \"$message_id\"";
  $query= $base_q . $q;
  $sth= $dbh->prepare($query);
  if (!$sth->execute)
  {
    print "QUERY: $query\n$DBI::errstr\n";
    $sth->finish;
    die;
  }
  while (($row= $sth->fetchrow_arrayref))
  {
    $mail_count++;
    for ($i= 0; $i < $fields; $i++)
    {
      if ($opt_message_id)
      {
	$mail[$fields][$mail_count]= $row->[$fields];
	$mail[$fields][$mail_count].= "\nNumber of Replies: " . get_nr_replies($row->[$fields]);
      }
      $mail[$i][$mail_count]= $row->[$i];
    }
    $new_message_id= $row->[$fields];
    if (defined($new_message_id) && length($new_message_id))
    {
      get_mail_by_message_id($new_message_id, $mail);
    }
  }
  return;
}

####
#### Get number of replies for a given message_id
####

sub get_nr_replies
{
  my ($message_id)= @_;
  my ($sth, $sth2, $q, $row, $row2, $nr_replies);

  $nr_replies= 0;
  $q= "SELECT COUNT(*) FROM my_mail WHERE in_reply_to=\"$message_id\"";
  $sth= $dbh->prepare($q);
  if (!$sth->execute)
  {
    print "QUERY: $q\n$DBI::errstr\n";
    $sth->finish;
    die;
  }
  while (($row= $sth->fetchrow_arrayref))
  {
    if (($nr_replies= $row->[0]))
    {
      $q= "SELECT message_id FROM my_mail WHERE in_reply_to=\"$message_id\"";
      $sth2= $dbh->prepare($q);
      if (!$sth2->execute)
      {
	print "QUERY: $q\n$DBI::errstr\n";
	$sth->finish;
	die;
      }
      while (($row2= $sth2->fetchrow_arrayref))
      {
	# There may be several replies to the same mail. Also the
	# replies to the 'parent' mail may contain several replies
	# and so on. Thus we need to calculate it recursively.
	$nr_replies+= get_nr_replies($row2->[0]);
      }
    }
    return $nr_replies;
  }
}

####
#### Print mails
####

sub print_mails
{
  my ($mail)= @_;
  my ($i);

  for ($i=0; $mail[0][$i]; $i++)
  {
    print "#" x 33;
    print " " . ($i+1) . ". Mail ";
    print "#" x 33;
    print "\n";
    if ($opt_message_id)
    {
      print "Msg ID: $mail[$fields][$i]\n";
    }
    print "From: $mail[0][$i]\n";
    print "To: $mail[1][$i]\n";
    print "Cc:" . (defined($mail[2][$i]) ? $mail[2][$i] : "") . "\n";
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
}  

####
#### Count mails that matches the query, but don't show them
####

sub count_mails
{
  my ($sth);

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
  --help       show this help
  --count      Shows how many mails matches the query, but not the mails.
  --db=        database to use (Default: $opt_db)
  --host=      Hostname which to connect (Default: $opt_host)
  --socket=    Unix socket to be used for connection (Default: $opt_socket)
  --password=  Password to use for mysql
  --user=      User to be used for mysql connection, if not current user
  --port=      mysql port to be used (Default: $opt_port)
  --thread     Will search for possible replies to emails found by the search
               criteria. Replies, if found, will be displayed right after the
               original mail.
  --message_id Display message_id on top of each mail. Useful when searching
               email threads with --thread. On the second line is the number
               of replies to the same thread, starting counting from that
               mail (excluding possible parent mails).
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
    Field       Type 
  - message_id  varchar(255) # Use with --thread and --message_id
  - in_reply_to varchar(255) # Internally used by --thread
  - mail_from   varchar(120)
  - date        datetime
  - sbj         varchar(200)
  - txt         mediumtext
  - cc          text
  - mail_to     text
  - time_zone   varchar(6)
  - reply       varchar(120)
  - file        varchar(32)
  - hash        int(11)
  An example of pmail:
  pmail "txt like '%libmysql.dll%' and sbj like '%delphi%'"
  NOTE: the txt field is NOT case sensitive!
EOF
  exit(0);
}
