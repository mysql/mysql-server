#!@PERL@
## Emacs, this is -*- perl -*- mode? :-)
##
##        Permission setter for MySQL
##
##        mady by Luuk de Boer (luuk@wxs.nl) 1998.
##        it's made under GPL ...:-))
##
##
############################################################################
## History
##
## 1.0 first start of the program
## 1.1 some changes from monty and after that
##     initial release in mysql 3.22.10 (nov 1998)
## 1.2 begin screen now in a loop + quit is using 0 instead of 9
##     after ideas of Paul DuBois.
## 1.2a Add Grant, References, Index and Alter privilege handling (Monty)

#### TODO
#
# empty ... suggestions ... mail them to me ...


$version="1.2";

use DBI;
use Getopt::Long;
use strict;
use vars qw($dbh $hostname $opt_user $opt_password $opt_help $opt_host
	    $opt_socket $opt_port $host $version);


$dbh=$host=$opt_user= $opt_password= $opt_help= $opt_host= $opt_socket= "";
$opt_port=0;

read_my_cnf();		# Read options from ~/.my.cnf

GetOptions("user=s","password=s","help","host=s","socket=s","port=i");

usage() if ($opt_help); # the help function

if ($opt_host eq '')
{
  $hostname = "localhost";
}
else
{
  $hostname = $opt_host;
}

# ask for a password if no password is set already
if ($opt_password eq '')
{
  system "stty -echo";
  print "Password for user $opt_user to connect to MySQL: ";
  $opt_password = <STDIN>;
  chomp($opt_password);
  system "stty echo";
  print "\n";
}


# make the connection to MySQL
$dbh= DBI->connect("DBI:mysql:mysql:host=$hostname:port=$opt_port:mysql_socket=$opt_socket",$opt_user,$opt_password, {PrintError => 0}) ||
  die("Can't make a connection to the mysql server.\n The error: $DBI::errstr");

# the start of the program
&q1();
exit(0); # the end...

#####
# below all subroutines of the program
#####

###
# the beginning of the program
###
sub q1 { # first question ...
  my ($answer,$end);
  while (! $end) {
    print "#"x70;
    print "\n";
    print "## Welcome to the permission setter $version for MySQL.\n";
    print "## made by Luuk de Boer\n";
    print "#"x70;
    print "\n";
    print "What would you like to do:\n";
    print "  1. Set password for a user.\n";
    print "  2. Add a database + user privilege for that database.\n";
    print "     - user can do all except all admin functions\n";
    print "  3. Add user privilege for an existing database.\n";
    print "     - user can do all except all admin functions\n";
    print "  4. Add user privilege for an existing database.\n";
    print "     - user can do all except all admin functions + no create/drop\n";
    print "  5. Add user privilege for an existing database.\n";
    print "     - user can do only selects (no update/delete/insert etc.)\n";
    print "  0. exit this program\n";
    print "\nMake your choice [1,2,3,4,5,0]: ";
    while (<STDIN>) {
      $answer = $_;
      chomp($answer);
      if ($answer =~ /1|2|3|4|5|0/) {
        &setpwd if ($answer == 1);
        &addall($answer) if ($answer =~ /^[2345]$/);
        if ($answer == 0) {
          print "Sorry, hope we can help you next time \n\n";
          $end = 1;
        }
      } else {
        print "Your answer was $answer\n";
        print "and that's wrong .... Try again\n";
      }
      last;
    }
  }
}

###
# set a password for a user
###
sub setpwd
{
  my ($user,$pass,$host);
  print "\n\nSetting a (new) password for a user.\n";

  $user = user();
  $pass = newpass($user);
  $host = hosts($user);

  print "#"x70;
  print "\n\n";
  print "That was it ... here is an overview of what you gave to me:\n";
  print "The username 		: $user\n";
#  print "The password		: $pass\n";
  print "The host		: $host\n";
  print "#"x70;
  print "\n\n";
  print "Are you pretty sure you would like to implement this [yes/no]: ";
  my $no = <STDIN>;
  chomp($no);
  if ($no =~ /n/i)
  {
    print "Okay .. that was it then ... See ya\n\n";
    return(0);
  }
  else
  {
    print "Okay ... let's go then ...\n\n";
  }
  $user = $dbh->quote($user);
  $host = $dbh->quote($host);
  if ($pass eq '')
  {
    $pass = "''";
  }
  else
  {
    $pass = "PASSWORD(". $dbh->quote($pass) . ")";
  }
  my $sth = $dbh->prepare("update user set Password=$pass where User = $user and Host = $host") || die $dbh->errstr;
  $sth->execute || die $dbh->errstr;
  $sth->finish;
  print "The password is set for user $user.\n\n";

}

###
# all things which will be added are done here
###
sub addall
{
  my ($todo) = @_;
  my ($answer,$good,$db,$user,$pass,$host,$priv);

  if ($todo == 2)
  {
    $db = newdatabase();
  }
  else
  {
    $db = database();
  }

  $user = newuser();
  $pass = newpass();
  $host = newhosts();

  print "#"x70;
  print "\n\n";
  print "That was it ... here is an overview of what you gave to me:\n";
  print "The database name	: $db\n";
  print "The username 		: $user\n";
#  print "The password		: $pass\n";
  print "The host(s)		: $host\n";
  print "#"x70;
  print "\n\n";
  print "Are you pretty sure you would like to implement this [yes/no]: ";
  my $no = <STDIN>;
  chomp($no);
  if ($no =~ /n/i)
  {
    print "Okay .. that was it then ... See ya\n\n";
    return(0);
  }
  else
  {
    print "Okay ... let's go then ...\n\n";
  }

  if ($todo == 2)
  {
    # create the database
    my $sth = $dbh->do("create database $db") || $dbh->errstr;
  }

  # select the privilege ....
  if (($todo == 2) || ($todo == 3))
  {
    $priv = "'Y','Y','Y','Y','Y','Y','Y','Y','Y','Y'";
  }
  elsif ($todo == 4)
  {
    $priv = "'Y','Y','Y','Y','N','N','N','Y','Y','Y'";
  }
  elsif ($todo == 5)
  {
    $priv = "'Y','N','N','N','N','N','N','N','N','N'";
  }
  else
  {
    print "Sorry, choice  number $todo isn't known inside the program .. See ya\n";
    quit();
  }

  my @hosts = split(/,/,$host);
  $user = $dbh->quote($user);
  $db = $dbh->quote($db);
  if ($pass eq '')
  {
    $pass = "''";
  }
  else
  {
    $pass = "PASSWORD(". $dbh->quote($pass) . ")";
  }
  foreach my $key (@hosts)
  {
    my $key1 = $dbh->quote($key);
    my $sth = $dbh->prepare("select Host,User from user where Host = $key1 and User = $user") || die $dbh->errstr;
    $sth->execute || die $dbh->errstr;
    my @r = $sth->fetchrow_array;
    if ($r[0])
    {
      print "WARNING WARNING SKIPPING CREATE FOR USER $user AND HOST $key\n";
      print "Reason: entry already exists in the user table.\n";
    }
    else
    {
      $sth = $dbh->prepare("insert into user (Host,User,Password) values($key1,$user,$pass)") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      $sth->finish;
    }
    $sth = $dbh->prepare("INSERT INTO db (Host,Db,User,Select_priv,Insert_priv,Update_priv,Delete_priv,Create_priv,Drop_priv,Grant_priv,References_priv,Index_priv,Alter_priv) VALUES ($key1,$db,$user,$priv)") || die $dbh->errstr;
    $sth->execute || die $dbh->errstr;
    $sth->finish;
  }
  $dbh->do("flush privileges") || print "Can't load privileges\n";
  print "Everything is inserted and mysql privileges have been reloaded.\n\n";
}

###
# ask for a new database name
###
sub newdatabase
{
  my ($answer,$good,$db);
  print "\n\nWhich database would you like to add: ";
  while (<STDIN>)
  {
    $answer = $_;
    $good = 0;
    chomp($answer);
    if ($answer)
    {
      my $sth = $dbh->prepare("show databases") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      while (my @r = $sth->fetchrow_array)
      {
        if ($r[0] eq $answer)
	{
          print "\n\nSorry, this database name is already in use; try something else: ";
          $good = 1;
        }
      }
    }
    else
    {
      print "You must type something ...\nTry again: ";
      next;
    }
    last if ($good == 0);
  }
  $db = $answer;
  print "The new database $db will be created\n";
  return($db);
}

###
# select a database
###
sub database
{
  my ($answer,$good,$db);
  print "\n\nWhich database would you like to select: \n";
  print "You can choose from: \n";
  my $sth = $dbh->prepare("show databases") || die $dbh->errstr;
  $sth->execute || die $dbh->errstr;
  while (my @r = $sth->fetchrow_array)
  {
    print "  - $r[0] \n";
  }
  print "Which database will it be (case sensitive): ";
  while (<STDIN>)
  {
    $answer = $_;
    $good = 0;
    chomp($answer);
    if ($answer)
    {
      my $sth = $dbh->prepare("show databases") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      while (my @r = $sth->fetchrow_array)
      {
        if ($r[0] eq $answer)
	{
          $good = 1;
          $db = $r[0];
          last;
        }
      }
    }
    else
    {
      print "You must type something ...\nTry again: ";
      next;
    }
    if ($good == 1)
    {
      last;
    }
    else
    {
      print "You must select one from the list.\nTry again: ";
      next;
    }
  }
  print "The database $db will be used.\n";
  return($db);
}

###
# ask for a new username
###
sub newuser
{
  my ($answer,$user);

  print "\nWhat username is to be created: ";
  while(<STDIN>)
  {
    $answer = $_;
    chomp($answer);
    if ($answer)
    {
      $user = $answer;
    }
    else
    {
      print "You must type something ...\nTry again: ";
      next;
    }
    last;
  }
  print "Username = $user\n";
  return($user);
}

###
# ask for a user which is already in the user table
###
sub user
{
  my ($answer,$user);

  print "\nFor which user do you want to specify a password: ";
  while(<STDIN>)
  {
    $answer = $_;
    chomp($answer);
    if ($answer)
    {
      my $sth = $dbh->prepare("select User from user where User = '$answer'") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      my @r = $sth->fetchrow_array;
      if ($r[0])
      {
        $user = $r[0];
      }
      else
      {
       print "Sorry, user $answer isn't known in the user table.\nTry again: ";
       next;
     }
    }
    else
    {
      print "You must type something ...\nTry again: ";
      next;
    }
    last;
  }
  print "Username = $user\n";
  return($user);
}

###
# ask for a new password
###
sub newpass
{
  my ($user) = @_;
  my ($answer,$good,$pass,$yes);

  print "Would you like to set a password for $user [y/n]: ";
  $yes = <STDIN>;
  chomp($yes);
  if ($yes =~ /y/)
  {
    system "stty -echo";
    print "What password do you want to specify for $user: ";
    while(<STDIN>)
    {
      $answer = $_;
      chomp($answer);
      system "stty echo";
      print "\n";
      if ($answer)
      {
        system "stty -echo";
        print "Type the password again: ";
        my $second = <STDIN>;
        chomp($second);
        system "stty echo";
        print "\n";
        if ($answer ne $second)
        {
          print "Passwords aren't the same; we begin from scratch again.\n";
          system "stty -echo";
          print "Password please: ";
          next;
        }
        else
        {
          $pass = $answer;
        }
      }
      else
      {
        print "You must type something ...\nTry again: ";
        next;
      }
      last;
    }
#    print "The password for $user is $pass.\n";
  }
  else
  {
    print "We won't set a password so the user doesn't have to use it\n";
    $pass = "";
  }
  return($pass);
}

###
# ask for new hosts
###
sub newhosts
{
  my ($answer,$good,$host);

  print "We now need to know from what host(s) the user will connect.\n";
  print "Keep in mind that % means 'from any host' ...\n";
  print "The host please: ";
  while(<STDIN>)
  {
    $answer = $_;
    chomp($answer);
    if ($answer)
    {
      $host .= ",$answer";
      print "Would you like to add another host [yes/no]: ";
      my $yes = <STDIN>;
      chomp($yes);
      if ($yes =~ /y/i)
      {
        print "Okay, give us the host please: ";
        next;
      }
      else
      {
        print "Okay we keep it with this ...\n";
      }
    }
    else
    {
      print "You must type something ...\nTry again: ";
      next;
    }
    last;
  }
  $host =~ s/^,//;
  print "The following host(s) will be used: $host.\n";
  return($host);
}

###
# ask for a host which is already in the user table
###
sub hosts
{
  my ($user) = @_;
  my ($answer,$good,$host);

  print "We now need to know which host for $user we have to change.\n";
  print "Choose from the following hosts: \n";
  $user = $dbh->quote($user);
  my $sth = $dbh->prepare("select Host,User from user where User = $user") || die $dbh->errstr;
  $sth->execute || die $dbh->errstr;
  while (my @r = $sth->fetchrow_array)
  {
    print "  - $r[0] \n";
  }
  print "The host please (case sensitive): ";
  while(<STDIN>)
  {
    $answer = $_;
    chomp($answer);
    if ($answer)
    {
      $sth = $dbh->prepare("select Host,User from user where Host = '$answer' and User = $user") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      my @r = $sth->fetchrow_array;
      if ($r[0])
      {
        $host = $answer;
        last;
      }
      else
      {
        print "You have to select a host from the list ...\nTry again: ";
        next;
      }
    }
    else
    {
      print "You have to type something ...\nTry again: ";
      next;
    }
    last;
  }
  print "The following host will be used: $host.\n";
  return($host);
}

###
# a nice quit (first disconnect and then exit
###
sub quit
{
  $dbh->disconnect;
  exit(0);
}

###
# Read variables password, port and socket from .my.cnf under the client
# or perl groups
###

sub read_my_cnf
{
  open(TMP,$ENV{'HOME'} . "/.my.cnf") || return 1;
  while (<TMP>)
  {
    if (/^\[(client|perl)\]/i)
    {
      while ((defined($_=<TMP>)) && !/^\[\w+\]/)
      {
	print $_;
	if (/^host\s*=\s*(\S+)/i)
	{
	  $opt_host = $1;
	}
	elsif (/^user\s*=\s*(\S+)/i)
	{
	  $opt_user = $1;
	}
	elsif (/^password\s*=\s*(\S+)/i)
	{
	  $opt_password = $1;
	}
	elsif (/^port\s*=\s*(\S+)/i)
	{
	  $opt_port = $1;
	}
	elsif (/^socket\s*=\s*(\S+)/i)
	{
	  $opt_socket = $1;
	}
      }
    }
  }
  close(TMP);
}

###
# the help text
###
sub usage
{
  print <<EOL;
----------------------------------------------------------------------
                 The permission setter for MySQL.
                      version: $version

                 made by: Luuk de Boer <luuk\@wxs.nl>
----------------------------------------------------------------------

The permission setter is a little program which can help you add users
or databases or change passwords in MySQL. Keep in mind that we don't
check permissions which already been set in MySQL. So if you can't
connect to MySQL using the permission you just added, take a look at
the permissions which have already been set in MySQL.

The permission setter first reads your .my.cnf file in your Home
directory if it exists.

Options for the permission setter:

--help		: print this help message and exit.

The options shown below are used for making the connection to the MySQL
server. Keep in mind that the permissions for the user specified via
these options must be sufficient to add users / create databases / set
passwords.

--user		: is the username to connect with.
--password	: the password of the username.
--host		: the host to connect to.
--socket	: the socket to connect to.
--port		: the port number of the host to connect to.

If you don't give a password and no password is set in your .my.cnf
file, then the permission setter will ask for a password.


EOL
exit(0);
}
