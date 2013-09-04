#!/usr/bin/perl
## Emacs, this is -*- perl -*- mode? :-)

# Copyright (c) 2000, 2007 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

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
## 1.3 Applied patch provided by Martin Mokrejs <mmokrejs@natur.cuni.cz>
##     (General code cleanup, use the GRANT statement instead of updating
##     the privilege tables directly, added option to revoke privileges)
## 1.4 Remove option 6 which attempted to erroneously grant global privileges

#### TODO
#
# empty ... suggestions ... mail them to me ...


$version="1.4";

use DBI;
use Getopt::Long;
use strict;
use vars qw($dbh $sth $hostname $opt_user $opt_password $opt_help $opt_host
	    $opt_socket $opt_port $host $version);

my $sqlhost = "";
my $user = "";

$dbh=$host=$opt_user= $opt_password= $opt_help= $opt_host= $opt_socket= "";
$opt_port=0;

read_my_cnf();		# Read options from ~/.my.cnf

GetOptions("user=s","password=s","help","host=s","socket=s","port=i");

usage() if ($opt_help); # the help function

if ($opt_host eq '')
{
  $sqlhost = "localhost";
}
else
{
  $sqlhost = $opt_host;
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
$dbh= DBI->connect("DBI:mysql:mysql:host=$sqlhost:port=$opt_port:mysql_socket=$opt_socket",$opt_user,$opt_password, {PrintError => 0}) ||
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
    print "  1. Set password for an existing user.\n";
    print "  2. Create a database + user privilege for that database\n";
		print "     and host combination (user can only do SELECT)\n";
    print "  3. Create/append user privilege for an existing database\n";
		print "     and host combination (user can only do SELECT)\n";
    print "  4. Create/append broader user privileges for an existing\n";
		print "     database and host combination\n";
		print "     (user can do SELECT,INSERT,UPDATE,DELETE)\n";
    print "  5. Create/append quite extended user privileges for an\n";
		print "     existing database and host combination (user can do\n";
		print "     SELECT,INSERT,UPDATE,DELETE,CREATE,DROP,INDEX,\n";
		print "     LOCK TABLES,CREATE TEMPORARY TABLES)\n";
    print "  6. Create/append full privileges for an existing database\n";
		print "     and host combination (user has FULL privilege)\n";
    print "  7. Remove all privileges for for an existing database and\n";
		print "     host combination.\n";
    print "     (user will have all permission fields set to N)\n";
    print "  0. exit this program\n";
    print "\nMake your choice [1,2,3,4,5,6,7,0]: ";
    while (<STDIN>) {
      $answer = $_;
      chomp($answer);
      if ($answer =~ /^[1234567]$/) {
        if ($answer == 1) {
	   setpwd();
        } elsif ($answer =~ /^[234567]$/) {
	   addall($answer);
	} else {
          print "Sorry, something went wrong. With such option number you should not get here.\n\n";
          $end = 1;
        }
      } elsif ($answer == 0) {
        print "We hope we can help you next time \n\n";
	$end = 1;
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
  my ($user,$pass,$host) = "";
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
sub addall {
  my ($todo) = @_;
  my ($answer,$good,$db,$user,$pass,$host,$priv);

  if ($todo == 2) {
    $db = newdatabase();
  } else {
    $db = database();
  }

  $user = newuser();
  $pass = newpass("$user");
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
  if ($no =~ /n/i) {
    print "Okay .. that was it then ... See ya\n\n";
    return(0);
  } else {
    print "Okay ... let's go then ...\n\n";
  }

  if ($todo == 2) {
    # create the database
    if ($db) {
    my $sth = $dbh->do("CREATE DATABASE $db") || $dbh->errstr;
    } else {
      print STDERR "What do you want? You wanted to create new database and add new user, right?\n";
      die "But then specify databasename, please\n";
  }
  }

  if ( ( !$todo ) or not ( $todo =~ m/^[2-7]$/ ) ) {
    print STDERR "Sorry, select option $todo isn't known inside the program .. See ya\n";
    quit();
  }

  my @hosts = split(/,/,$host);
  if (!$user) {
     die "username not specified: $user\n";
  }
  if (!$db) {
     die "databasename is not specified nor *\n";
  }
  foreach $host (@hosts) {
    # user privileges: SELECT
    if (($todo == 2) || ($todo == 3)) {
      $sth = $dbh->do("GRANT SELECT ON $db.* TO $user@\"$host\" IDENTIFIED BY \'$pass\'") || die $dbh->errstr;
    } elsif ($todo == 4) {
      # user privileges: SELECT,INSERT,UPDATE,DELETE
      $sth = $dbh->do("GRANT SELECT,INSERT,UPDATE,DELETE ON $db.* TO $user@\"$host\" IDENTIFIED BY \'$pass\'") || die $dbh->errstr;
    } elsif ($todo == 5) {
      # user privileges: SELECT,INSERT,UPDATE,DELETE,CREATE,DROP,INDEX,LOCK TABLES,CREATE TEMPORARY TABLES
      $sth = $dbh->do("GRANT SELECT,INSERT,UPDATE,DELETE,CREATE,DROP,INDEX,LOCK TABLES,CREATE TEMPORARY TABLES ON $db.* TO $user@\"$host\" IDENTIFIED BY \'$pass\'") || die $dbh->errstr;
    } elsif ($todo == 6) {
       # all privileges
       $sth = $dbh->do("GRANT ALL ON $db.* TO \'$user\'\@\'$host\' IDENTIFIED BY \'$pass\'") || die $dbh->errstr;
    } elsif ($todo == 7) {
       # all privileges set to N
       $sth = $dbh->do("REVOKE ALL ON $db.* FROM \'$user\'\@\'$host\'") || die $dbh->errstr;
    }
    }
  $dbh->do("FLUSH PRIVILEGES") || print STDERR "Can't flush privileges\n";
  print "Everything is inserted and mysql privileges have been reloaded.\n\n";
}

###
# ask for a new database name
###
sub newdatabase {
  my ($answer,$good,$db);
  print "\n\nWhich database would you like to add: ";
  while (<STDIN>) {
    $answer = $_;
    $good = 0;
    chomp($answer);
    if ($answer) {
      my $sth = $dbh->prepare("SHOW DATABASES") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      while (my @r = $sth->fetchrow_array) {
        if ($r[0] eq $answer) {
          print "\n\nSorry, this database name is already in use; try something else: ";
          $good = 1;
        }
      }
    } else {
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
sub database {
  my ($answer,$good,$db);
  print "\n\nWhich database from existing databases would you like to select: \n";
  print "You can choose from: \n";
  my $sth = $dbh->prepare("show databases") || die $dbh->errstr;
  $sth->execute || die $dbh->errstr;
  while (my @r = $sth->fetchrow_array) {
    print "  - $r[0] \n";
  }
  print "Which database will it be (case sensitive). Type * for any: \n";
  while (<STDIN>) {
    $answer = $_;
    $good = 0;
    chomp($answer);
    if ($answer) {
      if ($answer eq "*") {
        print "OK, the user entry will NOT be limited to any database";
        return("*");
      }
      my $sth = $dbh->prepare("show databases") || die $dbh->errstr;
      $sth->execute || die $dbh->errstr;
      while (my @r = $sth->fetchrow_array) {
        if ($r[0] eq $answer) {
          $good = 1;
          $db = $r[0];
          last;
        }
      }
    } else {
        print "Type either database name or * meaning any databasename. That means";
        print " any of those above but also any which will be created in future!";
        print " This option gives a user chance to operate on databse mysql, which";
        print " contains privilege settings. That is really risky!\n";
      next;
    }
    if ($good == 1) {
      last;
    } else {
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
  my $user = "";
  my $answer = "";

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
  my ($pass,$answer,$good,$yes);

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
  my ($host,$answer,$good);

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
