#!/usr/bin/perl -w
# Copyright Abandoned 1998 TCX DataKonsult AB & Monty Program KB & Detron HB
# This file is public domain and comes with NO WARRANTY of any kind
#
# This program is brought to you by Janne-Petteri Koilo with the 
# administration of Michael Widenius.
#
# Rewritten with a lot of bug fixes by Jani Tolonen and Thimble Smith
# 15.12.2000
#
# This program takes your mails and puts them into your database. It ignores
# messages with the same from, date and message text.
# You can use mail-files that are compressed or gzipped and ends with
# -.gz or -.Z.

use DBI;
use Getopt::Long;

$| = 1;
$VER = "2.3";

$opt_help          = 0;
$opt_version       = 0;
$opt_debug         = 0;
$opt_host          = undef();
$opt_port          = undef();
$opt_socket        = undef();
$opt_db            = undef();
$opt_table         = undef();
$opt_user          = undef();
$opt_password      = undef();
$opt_max_mail_size = 65536;
$opt_create        = 0;
$opt_test          = 0;
$opt_no_path       = 0;
$opt_stop_on_error = 0;
$opt_stdin         = 0;

my ($dbh, $progname, $mail_no_from_f, $mail_no_txt_f, $mail_too_big,
    $mail_forwarded, $mail_duplicates, $mail_no_subject_f, $mail_inserted);

$mail_no_from_f = $mail_no_txt_f = $mail_too_big = $mail_forwarded =
$mail_duplicates = $mail_no_subject_f = $mail_inserted = 0;
$mail_fixed=0;

#
# Remove the following message-ends from message
#
@remove_tail= (
"\n-*\nSend a mail to .*\n.*\n.*\$",
"\n-*\nPlease check .*\n.*\n\nTo unsubscribe, .*\n.*\n.*\nIf you have a broken.*\n.*\n.*\$",
"\n-*\nPlease check .*\n(.*\n){1,3}\nTo unsubscribe.*\n.*\n.*\$",
"\n-*\nPlease check .*\n.*\n\nTo unsubscribe.*\n.*\$",
"\n-*\nTo request this thread.*\nTo unsubscribe.*\n.*\.*\n.*\$",
"\n -*\n.*Send a mail to.*\n.*\n.*unsubscribe.*\$",
"\n-*\nTo request this thread.*\n\nTo unsubscribe.*\n.*\$"
);

# Generate regexp to remove tails where the unsubscribed is quoted
{
  my (@tmp, $tail);
  @tmp=();
  foreach $tail (@remove_tail)
  {
    $tail =~ s/\n/\n[> ]*/g;
    push(@tmp, $tail);
  }
  push @remove_tail,@tmp;
}

my %months = ('Jan' => 1, 'Feb' => 2, 'Mar' => 3, 'Apr' => 4, 'May' => 5,
	      'Jun' => 6, 'Jul' => 7, 'Aug' => 8, 'Sep' => 9, 'Oct' => 10,
	      'Nov' => 11, 'Dec' => 12);

$progname = $0;
$progname =~ s/.*[\/]//;

main();

####
#### main sub routine
####

sub main
{
  my ($connect_arg, @args, $ignored, @defops, $i);

  if (defined(my_which("my_print_defaults")))
  {
    @defops = `my_print_defaults mail_to_db`;
    chop @defops;
    splice @ARGV, 0, 0, @defops;
  }
  else
  {
    print "WARNING: No command 'my_print_defaults' found; unable to read\n";
    print "the my.cnf file. This command is available from the latest MySQL\n";
    print "distribution.\n";
  }
  GetOptions("help","version","host=s","port=i","socket=s","db=s","table=s",
	     "user=s","password=s","max_mail_size=i","create","test",
	     "no_path","debug","stop_on_error","stdin")
  || die "Wrong option! See $progname --help\n";

  usage($VER) if ($opt_help || $opt_version ||
		  (!$ARGV[0] && !$opt_create && !$opt_stdin));

  # Check that the given inbox files exist and are regular files
  for ($i = 0; ! $opt_stdin && defined($ARGV[$i]); $i++)
  {
    die "FATAL: Can't find inbox file: $ARGV[$i]\n" if (! -f $ARGV[$i]);
  }

  $connect_arg = "DBI:mysql:";
  push @args, "database=$opt_db" if defined($opt_db);
  push @args, "host=$opt_host" if defined($opt_host);
  push @args, "port=$opt_port" if defined($opt_port);
  push @args, "mysql_socket=$opt_socket" if defined($opt_socket);
  push @args, "mysql_read_default_group=mail_to_db";
  $connect_arg .= join ';', @args;
  $dbh = DBI->connect("$connect_arg", $opt_user, $opt_password,
		     { PrintError => 0})
  || die "Couldn't connect: $DBI::errstr\n";

  die "You must specify the database; use --db=" if (!defined($opt_db));
  die "You must specify the table; use --table=" if (!defined($opt_table));

  create_table($dbh) if ($opt_create);

  if ($opt_stdin)
  {
    open(FILE, "-");
    process_mail_file($dbh, "READ-FROM-STDIN");
  }
  else
  {
    foreach (@ARGV)
    {
      # Check if the file is compressed
      if (/^(.*)\.(gz|Z)$/)
      {
	open(FILE, "zcat $_ |");
	process_mail_file($dbh, $1);
      }
      else
      {
	open(FILE, $_);
	process_mail_file($dbh, $_);
      }
    }
  }
  $dbh->disconnect if (!$opt_test);

  $ignored = ($mail_no_from_f + $mail_no_subject_f + $mail_no_txt_f +
	      $mail_too_big + $mail_duplicates);
  print "Mails inserted:\t\t\t$mail_inserted\n";
  print "Mails ignored:\t\t\t$ignored\n";
  print "Mails without \"From:\" -field:\t$mail_no_from_f\n";
  print "Mails without message:\t\t$mail_no_txt_f\n";
  print "Mails without subject:\t\t$mail_no_subject_f\n";
  print "Too big mails (> $opt_max_mail_size):\t$mail_too_big\n";
  print "Duplicate mails:\t\t$mail_duplicates\n";
  print "Forwarded mails:\t\t$mail_forwarded\n";
  print "Total number of mails:\t\t"; 
  print $mail_inserted + $ignored;
  print "\n";
  print "Mails with unsubscribe removed:\t$mail_fixed\n";
  exit(0);
}

####
#### table creation
####

sub create_table
{
  my ($dbh) = @_;
  my ($sth, $query);

  $query = <<EOF;
CREATE TABLE $opt_table
(
 mail_id MEDIUMINT UNSIGNED NOT NULL auto_increment,
 date DATETIME NOT NULL,
 time_zone VARCHAR(20),
 mail_from VARCHAR(120) NOT NULL,
 reply VARCHAR(120),
 mail_to TEXT,
 cc TEXT,
 sbj VARCHAR(200),
 txt MEDIUMTEXT NOT NULL,
 file VARCHAR(64) NOT NULL,
 hash INTEGER NOT NULL,
 KEY (mail_id),
 PRIMARY KEY (mail_from, date, hash))
 TYPE=MyISAM COMMENT=''
EOF
  $sth = $dbh->prepare($query) or die $DBI::errstr;
  $sth->execute() or die "Couldn't create table: $DBI::errstr\n";
}

####
#### inbox processing. Can be either a real file, or standard input.
####

sub process_mail_file
{
  my ($dbh, $file_name) = @_;
  my (%values, $type, $check);

  $file_name =~ s/.*[\/]// if ($opt_no_path);

  %values = ();
  $type = "";
  $check = 0;

  while (<FILE>)
  {
    chop;
    if ($type ne "message")
    { 
      if (/^Reply-To: (.*)/i)
      {
	$type = "reply";
	$values{$type} = $1;
      }
      elsif (/^From: (.*)/i)
      {
	$type = "from";
	$values{$type} = $1;
      }
      elsif (/^To: (.*)/i)
      {
	$type = "to";
	$values{$type} = $1;
      }
      elsif (/^Cc: (.*)/i)
      {
	$type = "cc";
	$values{$type} = $1;
      }
      elsif (/^Subject: (.*)/i)
      {
	$type = "subject";
	$values{$type} = $1;
      }
      elsif (/^Date: (.*)/i)
      {
	date_parser($1, \%values, $file_name);
	$type = "rubbish";
      }
      elsif (/^[\w\W-]+:\s/)
      {
	$type = "rubbish";  
      }
      elsif ($_ eq "")
      { 
	$type = "message";
	$values{$type} = "";
      }
      else
      {
	s/^\s*/ /;
	$values{$type} .= $_;
      }
    }
    elsif ($check != 0 && $_ ne "") # in case of forwarded messages
    {
      $values{$type} .= "\n" . $_;
      $check--;
    }
    elsif (/^From .* \d\d:\d\d:\d\d\s\d\d\d\d$/)
    {
      $values{'hash'} = checksum("$values{'message'}");
      update_table($dbh, $file_name, \%values);
      %values = ();
      $type = "";
      $check = 0;
    }
    elsif (/-* forwarded message .*-*/i) # in case of forwarded messages
    {
      $values{$type} .= "\n" . $_;
      $check++;
      $mail_forwarded++;
    }
    else
    {
      $values{$type} .= "\n" . $_;
    }
  }
  $values{'hash'} = checksum("$values{'message'}");
  update_table($dbh, $file_name, \%values);
}

####
#### get date and timezone
####

sub date_parser
{
  my ($date_raw, $values, $file_name, $tmp) = @_;

  # If you ever need to change this test, be especially careful with
  # the timezone; it may be just a number (-0600), or just a name (EET), or
  # both (-0600 (EET), or -0600 (EET GMT)), or without parenthesis: GMT.
  # You probably should use a 'greedy' regexp in the end
  $date_raw =~ /^\D*(\d{1,2})\s+(\w+)\s+(\d{2,4})\s+(\d+:\d+)(:\d+)?\s*(\S+.*)?/;

  if (!defined($1) || !defined($2) || !defined($3) || !defined($4) ||
      !defined($months{$2}))
  {
    if ($opt_debug || $opt_stop_on_error)
    {
      print "FAILED: date_parser: 1: $1 2: $2 3: $3 4: $4 5: $5\n";
      print "months{2}: $months{$2}\n";
      print "date_raw: $date_raw\n";
      print "Inbox filename: $file_name\n";
    }
    exit(1) if ($opt_stop_on_error);
    $values->{'date'} = "";
    $values->{'time_zone'} = "";
    return;
  }
  $tmp = $3 . "-" . $months{$2} . "-" . "$1 $4";
  $tmp.= defined($5) ? $5 : ":00";
  $values->{'date'} = $tmp;
  print "INSERTING DATE: $tmp\n" if ($opt_debug);
  $values->{'time_zone'} = $6;
}

####
#### Insert to table
#### 

sub update_table
{
  my($dbh, $file_name, $values) = @_;
  my($q,$tail,$message);

  if (!defined($values->{'subject'}) || !defined($values->{'to'}))
  {
    $mail_no_subject_f++;
    return;			# Ignore these
  }
  $message=$values->{'message'};
  $message =~ s/^\s*//; #removes whitespaces from the beginning 

 restart:
  $message =~ s/[\s\n>]*$//; #removes whitespaces and '>' from the end
  $values->{'message'}=$message;
  foreach $tail (@remove_tail)
  {
    $message =~ s/$tail//;
  }
  if ($message ne $values->{'message'})
  {
    $message =~ s/\s*$//; #removes whitespaces from the end
    $mail_fixed++;
    goto restart;	  # Some mails may have duplicated messages
  }

  $q = "INSERT INTO $opt_table (";
  $q.= "mail_id,";
  $q.= "date,";
  $q.= "time_zone,";
  $q.= "mail_from,";
  $q.= "reply,";
  $q.= "mail_to,";
  $q.= "cc,";
  $q.= "sbj,";
  $q.= "txt,";
  $q.= "file,";
  $q.= "hash";
  $q.= ") VALUES (";
  $q.= "NULL,";
  $q.= "'" . $values->{'date'} . "',";
  $q.= (defined($values->{'time_zone'}) ?
	$dbh->quote($values->{'time_zone'}) : "NULL");
  $q.= ",";
  $q.= defined($values->{'from'}) ? $dbh->quote($values->{'from'}) : "NULL";
  $q.= ",";
  $q.= defined($values->{'reply'}) ? $dbh->quote($values->{'reply'}) : "NULL";
  $q.= ",";
  $q.= defined($values->{'to'}) ? $dbh->quote($values->{'to'}) : "NULL";
  $q.= ",";
  $q.= defined($values->{'cc'}) ? $dbh->quote($values->{'cc'}) : "NULL"; 
  $q.= ","; 
  $q.= $dbh->quote($values->{'subject'});
  $q.= ",";
  $q.= $dbh->quote($message);
  $q.= ",";
  $q.= $dbh->quote($file_name);
  $q.= ",";
  $q.= "'" . $values->{'hash'} . "'";
  $q.= ")";

  # Don't insert mails bigger than $opt_max_mail_size
  if (length($message) > $opt_max_mail_size)
  {
    $mail_too_big++;
  }
  # Don't insert mails without 'From' field
  elsif (!defined($values->{'from'}) || $values->{'from'} eq "")
  {
    $mail_no_from_f++;
  }
  elsif ($opt_test)
  {
    print "$q\n";
    $mail_inserted++;
  }
  # Don't insert mails without the 'message'
  elsif ($message eq "") 
  {
    $mail_no_txt_f++;
  }
  elsif ($dbh->do($q))
  {
    $mail_inserted++;
  }
  # This should never happen. This means that the above q failed,
  # but it wasn't because of a duplicate mail entry
  elsif (!($DBI::errstr =~ /Duplicate entry /))
  {
    die "FATAL: Got error :$DBI::errstr\nAttempted query was: $q\n";
  }
  else
  {
    $mail_duplicates++;
    print "Duplicate mail: query: $q\n" if ($opt_debug);
  }
  $q = "";
}

####
#### In case you have two identical messages we wanted to identify them
#### and remove additionals;  We do this by calculating a hash number of the
#### message and ignoring messages with the same from, date and hash.
#### This function calculates a simple 32 bit hash value for the message.
####

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

####
#### my_which is used, because we can't assume that every system has the
#### which -command. my_which can take only one argument at a time.
#### Return values: requested system command with the first found path,
#### or undefined, if not found.
####

sub my_which
{
  my ($command) = @_;
  my (@paths, $path);

  return $command if (-f $command && -x $command);
  @paths = split(':', $ENV{'PATH'});
  foreach $path (@paths)
  {
    $path = "." if ($path eq "");
    $path .= "/$command";
    return $path if (-f $path && -x $path);
  }
  return undef();
}

####
#### usage and version
####

sub usage
{  
  my ($VER)= @_;
  
  if ($opt_version)
  {
    print "$progname version $VER\n";
  } 
  else
  {
    print <<EOF;
$progname version $VER

Description: Insert mails from inbox file(s) into a table. This program 
can read group [mail_to_db] from the my.cnf file. You may want to have db
and table set there at least.

Usage: $progname [options] file1 [file2 file3 ...]
or:    $progname [options] --create [file1 file2...]
or:    cat inbox | $progname [options] --stdin

The last example can be used to read mails from standard input and can
useful when inserting mails to database via a program 'on-the-fly'.
The filename will be 'READ-FROM-STDIN' in this case.

Options:
--help             Show this help and exit.
--version          Show the version number and exit.
--debug            Print some extra information during the run.
--host=...         Hostname to be used.
--port=#           TCP/IP port to be used with connection.
--socket=...       MySQL UNIX socket to be used with connection.
--db=...           Database to be used.
--table=...        Table name for mails.
--user=...         Username for connecting.
--password=...     Password for the user.
--stdin            Read mails from stdin.
--max_mail_size=#  Maximum size of a mail in bytes.
                   Beware of the downside letting this variable be too big;
                   you may easily end up inserting a lot of attached 
                   binary files (like MS Word documents etc), which take
                   space, make the database slower and are not really
                   searchable anyway. (Default $opt_max_mail_size)
--create           Create the mails table. This can be done with the first run.
--test		   Dry run. Print the queries and the result as it would be.
--no_path          When inserting the file name, leave out any paths of
                   the name.
--stop_on_error    Stop the run, if an unexpected, but not fatal error occurs
                   during the run. Without this option some fields may get
                   unwanted values. --debug will also report about these.
EOF
  }
  exit(0);
}
