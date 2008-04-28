#!/usr/bin/perl
# -*- cperl -*-
#
# Copyright (C) 2002 MySQL AB and Jeremy Cole
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

use Fcntl;
use strict;

my $config  = ".my.cnf.$$";
my $command = ".mysql.$$";
my $hadpass = 0;

# FIXME
# trap "interrupt" 2

my $rootpass = "";

sub echo_on {
  if ($^O eq 'MSWin32') {
    ReadMode('normal');
  } else {
    system("stty echo");
  }
}

sub echo_off {
  if ($^O eq 'MSWin32') {
    ReadMode('noecho');
  } else {
    system("stty -echo");
  }
}

sub write_file {
  my $file = shift;
  -f $file or die "ERROR: file is missing \"$file\": $!";
  open(FILE, ">$file") or die "ERROR: can't write to file \"$file\": $!";
  foreach my $line ( @_ ) {
    print FILE $line, "\n";             # Add EOL char
  }
  close FILE;
}

sub prepare {
  foreach my $file ( $config, $command ) {
    next if -f $file;                   # Already exists
    local *FILE;
    sysopen(FILE, $file, O_CREAT, 0600)
      or die "ERROR: can't create $file: $!";
    close FILE;
  }
}

sub do_query {
  my $query   = shift;
  write_file($command, $query);
  system("mysql --defaults-file=$config < $command");
  return $?;
}

sub make_config {
  my $password = shift;

  write_file($config,
             "# mysql_secure_installation config file",
             "[mysql]",
             "user=root",
             "password=$rootpass");
}

sub get_root_password {
  my $status = 1;
  while ( $status == 1 ) {
    echo_off();
    print "Enter current password for root (enter for none): ";
    my $password = <STDIN>;
    echo_on();
    if ( $password ) {
      $hadpass = 1;
    } else {
      $hadpass = 0;
    }
    $rootpass = $password;
    make_config($rootpass);
    do_query("");
    $status = $?;
  }
  print "OK, successfully used password, moving on...\n\n";
}

sub set_root_password {
  echo_off();
  print "New password: ";
  my $password1 = <STDIN>;
  print "\nRe-enter new password: ";
  my $password2 = <STDIN>;
  print "\n";
  echo_on();

  if ( $password1 eq $password2 ) {
    print "Sorry, passwords do not match.\n\n";
    return 1;
  }

  if ( !$password1 ) {
    print "Sorry, you can't use an empty password here.\n\n";
    return 1;
  }

  do_query("UPDATE mysql.user SET Password=PASSWORD('$password1') WHERE User='root';");
  if ( $? == 0 ) {
    print "Password updated successfully!\n";
    print "Reloading privilege tables..\n";
    if ( !reload_privilege_tables() ) {
      exit 1;
    }
    print "\n";
    $rootpass = $password1;
    make_config($rootpass);
  } else {
    print "Password update failed!\n";
    exit 1;
  }

  return 0;
}

sub remove_anonymous_users {
  do_query("DELETE FROM mysql.user WHERE User='';");
  if ( $? == 0 ) {
    print " ... Success!\n";
  } else {
    print " ... Failed!\n";
    exit 1;
  }

  return 0;
}

sub remove_remote_root {
  do_query("DELETE FROM mysql.user WHERE User='root' AND Host!='localhost';");
  if ( $? == 0 ) {
    print " ... Success!\n";
  } else {
    print " ... Failed!\n";
  }
}

sub remove_test_database {
  print " - Dropping test database...\n";
  do_query("DROP DATABASE test;");
  if ( $? == 0 ) {
    print " ... Success!\n";
  } else {
    print " ... Failed!  Not critical, keep moving...\n";
  }

  print " - Removing privileges on test database...\n";
  do_query("DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%'");
  if ( $? == 0 ) {
    print " ... Success!\n";
  } else {
    print " ... Failed!  Not critical, keep moving...\n";
  }

  return 0;
}

sub reload_privilege_tables {
  do_query("FLUSH PRIVILEGES;");
  if ( $? == 0 ) {
    print " ... Success!\n";
    return 0;
  } else {
    print " ... Failed!\n";
    return 1;
  }
}

sub interrupt {
  print "\nAborting!\n\n";
  cleanup();
  echo_on();
  exit 1;
}

sub cleanup {
  print "Cleaning up...\n";
  unlink($config,$command);
}


# The actual script starts here

prepare();

print <<HERE;



NOTE: RUNNING ALL PARTS OF THIS SCRIPT IS RECOMMENDED FOR ALL MySQL
      SERVERS IN PRODUCTION USE!  PLEASE READ EACH STEP CAREFULLY!

In order to log into MySQL to secure it, we'll need the current
password for the root user.  If you've just installed MySQL, and
you haven't set the root password yet, the password will be blank,
so you should just press enter here.

HERE

get_root_password();


#
# Set the root password
#

print "Setting the root password ensures that nobody can log into the MySQL\n";
print "root user without the proper authorisation.\n\n";

if ( $hadpass == 0 ) {
  print "Set root password? [Y/n] ";
} else {
  print "You already have a root password set, so you can safely answer 'n'.\n\n";
  print "Change the root password? [Y/n] ";
}

my $reply = <STDIN>;
if ( $reply =~ /n/i ) {
  print " ... skipping.\n";
} else {
  my $status = 1;
  while ( $status == 1 ) {
    set_root_password();
    $status = $?;
  }
}
print "\n";


#
# Remove anonymous users
#

print <<HERE;
By default, a MySQL installation has an anonymous user, allowing anyone
to log into MySQL without having to have a user account created for
them.  This is intended only for testing, and to make the installation
go a bit smoother.  You should remove them before moving into a
production environment.

HERE

print "Remove anonymous users? [Y/n] ";
$reply = <STDIN>;
if ( $reply =~ /n/i ) {
  print " ... skipping.\n";
} else {
  remove_anonymous_users();
}
print "\n";


#
# Disallow remote root login
#

print <<HERE;
Normally, root should only be allowed to connect from 'localhost'.  This
ensures that someone cannot guess at the root password from the network.

HERE

print "Disallow root login remotely? [Y/n] ";
$reply = <STDIN>;
if ( $reply =~ /n/i ) {
  print " ... skipping.\n";
} else {
  remove_remote_root();
}
print "\n";


#
# Remove test database
#

print <<HERE;
By default, MySQL comes with a database named 'test' that anyone can
access.  This is also intended only for testing, and should be removed
before moving into a production environment.

HERE

print "Remove test database and access to it? [Y/n] ";
$reply = <STDIN>;
if ( $reply =~ /n/i ) {
  print " ... skipping.\n";
} else {
  remove_test_database();
}
print "\n";


#
# Reload privilege tables
#

print <<HERE;
Reloading the privilege tables will ensure that all changes made so far
will take effect immediately.

HERE

print "Reload privilege tables now? [Y/n] ";
$reply = <STDIN>;
if ( $reply =~ /n/i ) {
  print " ... skipping.\n";
} else {
  reload_privilege_tables();
}
print "\n";

cleanup();

print <<HERE;



All done!  If you've completed all of the above steps, your MySQL
installation should now be secure.

Thanks for using MySQL!


HERE



