#!/usr/bin/perl
# Copyright Abandoned 1998 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind
#
# This program is brought to you by Janne-Petteri Koilo with the 
# administration of Michael Widenius.

# This program takes your mails and puts them into your database. It ignores
# messages with the same from, date and message text.
# You can use mail-files that are compressed or gzipped and ends with
# -.gz or -.Z.

use DBI;
use Getopt::Long;

$VER = "1.6";

$opt_db = "mail";
$opt_table = "mails";
$opt_max_mail_size = 65536;
$opt_db_engine = "mysql";
$opt_host = "localhost";
$opt_user = $opt_password = "";
$opt_help = $opt_version = $opt_test=0;

GetOptions("help","version","user=s","password=s",
	   "db_engine=s","db=s","host=s","max_mail_size=s","test") || usage();

usage($VER) if ($opt_help || $opt_version || !$ARGV[0]);

%months= ('Jan' => 1, 'Feb' => 2, 'Mar' => 3, 'Apr' => 4, 'May' => 5,
	  'Jun' => 6, 'Jul' => 7, 'Aug' => 8, 'Sep' => 9, 'Oct' => 10,
	  'Nov' => 11, 'Des' => 12);

$count_no_from = $count_no_txt = $count_too_big = 0;
$count_forwarded_msgs = $count_duplicates = $no_subject = 0;
$inserted_mails = 0;
$dbh=0;

$dbh = DBI->connect("DBI:$opt_db_engine:$opt_db:$opt_host",$opt_user,
		    $opt_password,{ PrintError => 0}) || die $DBI::errstr;
if (!$opt_test)
{
  create_table_if_needed($dbh);
}

foreach (@ARGV)
{
  if (/^(.*)\.(gz|Z)$/) #checks if the file is compressed or gzipped
  {
    open(FILE, "zcat $_ |");
    process_mail_file($dbh,$1);
  }
  else
  {
    open(FILE,$_);
    process_mail_file($dbh,$_);
  }
}
$dbh->disconnect if (!$opt_test);

$ignored = $count_no_from + $count_no_txt + $count_too_big + $count_duplicates + $no_subject;
print "Mails inserted:\t\t\t$inserted_mails\n";
print "Mails ignored:\t\t\t$ignored\n";
print "Mails without \"From:\" -field:\t$count_no_from\n";
print "Mails without message:\t\t$count_no_txt\n";
print "Too big mails (> $opt_max_mail_size):\t$count_too_big\n";
print "Duplicate mails:\t\t$count_duplicates\n";
print "Forwarded mails:\t\t$count_forwarded_msgs\n";
print "No subject:\t\t\t$no_subject\n";
print "Mails altogether:\t\t"; 
print $inserted_mails+$ignored;
print "\n";
exit(0);

sub usage
{  
  my($VER)=@_;
  
  $0 =~ s/.\/(.+)/$1/;
  if ($opt_version)
  {
    print "$0 version $VER\n";
  } 
  else
  {
    print <<EOF;
$0 version $VER

Usage: $0 [options] file1 [file2 file3 ...]

Description: Inserts mails from file(s) into a database

Options:
--help             show this help and exit
--version          shows the version of the program
--db_engine=...    database server (default: $opt_db_engine)
--db=...           database to be used (default: $opt_db)
--host=...         hostname to be used (default: $opt_host)
--password=...     user password for the db server
--user=...         username for the db server
--max_mail_size=#  max size of a mail to be inserted into the db.
                   mail will be ignored if it exceeds this size
                   (default $opt_max_mail_size)
--test		   Don\'t connect to the database, just write the
		   queries to stdout
EOF
  }
  exit(0);
}

sub create_table_if_needed
{
  my ($dbh)=@_;
  my ($sth,$create);
  
  $sth = $dbh->prepare("select count(*) from $opt_table") or die $dbh->errstr;
  if (!$sth->execute)
  {
    $create = "CREATE TABLE $opt_table (msg_nro mediumint unsigned not null ";
    $create .= "auto_increment, date DATETIME NOT NULL, time_zone CHAR(6) ";
    $create .= "NOT NULL, mail_from char(120) not null, reply char(120), ";
    $create .= "mail_to TEXT, cc TEXT, sbj char(200), txt MEDIUMTEXT NOT ";
    $create .= "NULL, file char(32) noT NULL, hash INT NOT NULL, key ";
    $create .= "(msg_nro), primary key (mail_from, date, time_zone, hash))";
    $sth = $dbh->prepare($create) or die $dbh->errstr;
    $sth->execute() or die $dbh->errstr;
  }  
}

sub process_mail_file
{
  my ($dbh,$file_name)= @_;
  my (%values,$type,$check);

  %values=(); $type="";
  $check=0;

  while (<FILE>)
  {
    chop;
    if ($type ne "message")
    { 
      if (/^Reply-To: (.*)/i)  # finding different fields from file
      {
	$type="reply";
	$values{$type}= $1;
      }
      elsif (/^From: (.*)/i)
      {
	$type="from";
	$values{$type}= $1;
      }
      elsif (/^To: (.*)/i)
      {
	$type="to";
	$values{$type}= $1;
      }
      elsif (/^Cc: (.*)/i)
      {
	$type="cc";
	$values{$type}= $1;
      }
      elsif (/^Subject: (.*)/i)
      {
	$type="subject";
	$values{$type}= $1;
      }
      elsif (/^Date: (.*)/i)
      {
	date_parser($1,\%values);
	$type="rubbish";
      }
      elsif (/^[\w\W-]+:\s/)
      {
	$type="rubbish";  
      }
      elsif ($_ eq "")
      { 
	$type="message";
	$values{$type}="";
      }
      else
      {
	s/^\s*/ /;
	$values{$type}.= $_;
      }
    }
    elsif ($check!=0 && $_ ne "") # in case of forwarded messages
    {
      $values{$type}.= "\n" . $_;
      $check--;
    }
    elsif (/^From .* \d\d:\d\d:\d\d\s\d\d\d\d$/)
    {
      $values{'hash'}= checksum("$values{'message'}");
      update_table($dbh,$file_name,\%values);
      %values=(); $type="";
      $check=0;
    }
    elsif (/-* forwarded message .*-*/i) # in case of forwarded messages
    {
      $values{$type}.= "\n" . $_;
      $check++;
      $count_forwarded_msgs++;
    }
    else
    {
      $values{$type}.= "\n" . $_;
    }
  }
  $values{'hash'}= checksum("$values{'message'}");
  update_table($dbh,$file_name,\%values);
}

########

# converts date to the right form

sub date_parser
{
  my ($date_raw,$values)=@_;

  $date_raw =~ /\s*(\d{1,2}) (\w+) (\d{2,4}) (\d+:\d+:\d+)\s*([\w-+]{3-5})?/;

  $values->{'date'}=$3 . "-" . $months{$2} . "-" . "$1 $4";
  $values->{'time_zone'}=$5;
}

#########

# this is runned when the whole mail is gathered.
# this actually puts the mail to the database.

sub update_table
{
  my($dbh,$file_name,$values)=@_;
  my($query);

  if (! defined($values->{'subject'}) || !defined($values->{'to'}))
  {
    $no_subject++;
    return;			# Ignore these
  }
  $values->{'message'} =~ s/^\s*//; #removes whitespaces from the beginning 
  $values->{'message'} =~ s/\s*$//; #removes whitespaces from the end
  $query = "insert into $opt_table values (NULL,'" . $values->{'date'};
  $query .= "','" . $values->{'time_zone'} . "',";
  $query .= (defined($values->{'from'}) ? $dbh->quote($values->{'from'}) : "NULL") . ",";
  $query .= (defined($values->{'reply'}) ? $dbh->quote($values->{'reply'}) : "NULL") . ",";

  $query .= (defined($values->{'to'}) ? $dbh->quote($values->{'to'}) : "NULL") . ","; 
  $query .= (defined($values->{'cc'}) ? $dbh->quote($values->{'cc'}) : "NULL") . ","; 
  $query .= $dbh->quote($values->{'subject'}) . ",";
  $query .= $dbh->quote($values->{'message'}) . "," . $dbh->quote($file_name);
  $query .= ",'" . $values->{'hash'} . "')";
  
  if (length($values->{'message'}) > $opt_max_mail_size) #disables big message
  {
    $count_too_big++;
  }
  elsif ($values->{'from'} eq "") #disables mails with no from field
  {
    $count_no_from++;
  }
  elsif ($opt_test)
  {
    print "$query\n";
    $inserted_mails++;
  }
  elsif ($values->{'message'} eq "") #disables mails with no message text
  {
    $count_no_msg_text++;
  }
  elsif ($dbh->do($query))
  {
    $inserted_mails++;
  }
  elsif (!($dbh->errstr =~ /Duplicate entry /)) #disables duplicates
  {
    die "Aborting: Got error '" . $dbh->errstr ."' for query: '$query'\n";
  }
  else
  {
    $count_duplicates++;    
  }
  $query="";
}


##########

# In case you have two identical messages we wanted to identify them
# and remove additionals;  We do this by calculating a hash number of the
# message and ignoring messages with the same from, date and hash.
# This function calculates a simple 32 bit hash value for the message.

sub checksum
{
  my ($txt)= @_;
  my ($crc,$i,$count);
  $count = length($txt);
  for ($crc = $i = 0; $i < $count ; $i++)
  {
    $crc = (($crc << 1) + (ord (substr ($txt, $i, 1)))) +
      (($crc & (1 << 30)) ? 1 : 0);
    $crc &= ((1 << 31) -1);
  }
  return $crc;
}
