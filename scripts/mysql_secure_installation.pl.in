#!/usr/bin/perl
# -*- cperl -*-
#
# Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

use Fcntl;
use File::Spec;
use if $^O eq 'MSWin32', 'Term::ReadKey' => qw/ReadMode/;
use strict;

my $config  = ".my.cnf.$$";
my $command = ".mysql.$$";
my $hadpass = 0;
my $mysql;  # How to call the mysql client
my $rootpass = "";


$SIG{QUIT} = $SIG{INT} = sub {
  print "\nAborting!\n\n";
  echo_on();
  cleanup();
  exit 1;
};


END {
  # Remove temporary files, even if exiting via die(), etc.
  cleanup();
}


sub read_without_echo {
  my ($prompt) = @_;
  print $prompt;
  echo_off();
  my $answer = <STDIN>;
  echo_on();
  print "\n";
  chomp($answer);
  return $answer;
}

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
  # Locate the mysql client; look in current directory first, then
  # in path
  our $SAVEERR;   # Suppress Perl warning message
  open SAVEERR, ">& STDERR";
  close STDERR;
  for my $m (File::Spec->catfile('bin', 'mysql'), 'mysql') {
    # mysql --version should always work
    qx($m --no-defaults --version);
    next unless $? == 0;

    $mysql = $m;
    last;
  }
  open STDERR, ">& SAVEERR";

  die "Can't find a 'mysql' client in PATH or ./bin\n"
    unless $mysql;

  # Create safe files to avoid leaking info to other users
  foreach my $file ( $config, $command ) {
    next if -f $file;                   # Already exists
    local *FILE;
    sysopen(FILE, $file, O_CREAT, 0600)
      or die "ERROR: can't create $file: $!";
    close FILE;
  }
}

# Simple escape mechanism (\-escape any ' and \), suitable for two contexts:
# - single-quoted SQL strings
# - single-quoted option values on the right hand side of = in my.cnf
#
# These two contexts don't handle escapes identically.  SQL strings allow
# quoting any character (\C => C, for any C), but my.cnf parsing allows
# quoting only \, ' or ".  For example, password='a\b' quotes a 3-character
# string in my.cnf, but a 2-character string in SQL.
#
# This simple escape works correctly in both places.
sub basic_single_escape {
  my ($str) = @_;
  # Inside a character class, \ is not special; this escapes both \ and '
  $str =~ s/([\'])/\\$1/g;
  return $str;
}

sub do_query {
  my $query   = shift;
  write_file($command, $query);
  my $rv = system("$mysql --defaults-file=$config < $command");
  # system() returns -1 if exec fails (e.g., command not found, etc.); die
  # in this case because nothing is going to work
  die "Failed to execute mysql client '$mysql'\n" if $rv == -1;
  # Return true if query executed OK, or false if there was some problem
  # (for example, SQL error or wrong password)
  return ($rv == 0 ? 1 : undef);
}

sub make_config {
  my $password = shift;

  my $esc_pass = basic_single_escape($rootpass);
  write_file($config,
             "# mysql_secure_installation config file",
             "[mysql]",
             "user=root",
             "password='$esc_pass'");
}

sub get_root_password {
  my $attempts = 3;
  for (;;) {
    my $password = read_without_echo("Enter current password for root (enter for none): ");
    if ( $password ) {
      $hadpass = 1;
    } else {
      $hadpass = 0;
    }
    $rootpass = $password;
    make_config($rootpass);
    last if do_query("");

    die "Unable to connect to the server as root user, giving up.\n"
      if --$attempts == 0;
  }
  print "OK, successfully used password, moving on...\n\n";
}

sub set_root_password {
  my $password1;
  for (;;) {
    $password1 = read_without_echo("New password: ");

    if ( !$password1 ) {
      print "Sorry, you can't use an empty password here.\n\n";
      next;
    }

    my $password2 = read_without_echo("Re-enter new password: ");

    if ( $password1 ne $password2 ) {
      print "Sorry, passwords do not match.\n\n";
      next;
    }

    last;
  }

  my $esc_pass = basic_single_escape($password1);
  do_query("UPDATE mysql.user SET Password=PASSWORD('$esc_pass') WHERE User='root';")
    or die "Password update failed!\n";

  print "Password updated successfully!\n";
  print "Reloading privilege tables..\n";
  reload_privilege_tables()
    or die "Can not continue.\n";

  print "\n";
  $rootpass = $password1;
  make_config($rootpass);
}

sub remove_anonymous_users {
  do_query("DELETE FROM mysql.user WHERE User='';")
    or die print " ... Failed!\n";
  print " ... Success!\n";
}

sub remove_remote_root {
  if (do_query("DELETE FROM mysql.user WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');")) {
    print " ... Success!\n";
  } else {
    print " ... Failed!\n";
  }
}

sub remove_test_database {
  print " - Dropping test database...\n";
  if (do_query("DROP DATABASE test;")) {
    print " ... Success!\n";
  } else {
    print " ... Failed!  Not critical, keep moving...\n";
  }

  print " - Removing privileges on test database...\n";
  if (do_query("DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%'")) {
    print " ... Success!\n";
  } else {
    print " ... Failed!  Not critical, keep moving...\n";
  }
}

sub reload_privilege_tables {
  if (do_query("FLUSH PRIVILEGES;")) {
    print " ... Success!\n";
    return 1;
  } else {
    print " ... Failed!\n";
    return undef;
  }
}

sub cleanup {
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
  set_root_password();
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

print <<HERE;



All done!  If you've completed all of the above steps, your MySQL
installation should now be secure.

Thanks for using MySQL!


HERE



