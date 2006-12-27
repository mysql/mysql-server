#-----------------------------------------------------------------------------
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
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# This notice applies to changes, created by or for Novell, Inc., 
# to preexisting works for which notices appear elsewhere in this file. 
 
# Copyright (c) 2003 Novell, Inc. All Rights Reserved. 

# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License as published by 
# the Free Software Foundation; either version 2 of the License, or 
# (at your option) any later version. 

# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# GNU General Public License for more details. 

# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#-----------------------------------------------------------------------------

use strict;
use Mysql;

print "MySQL Secure Installation Script\n\n";

print "NOTE: RUNNING ALL PARTS OF THIS SCRIPT IS RECOMMENDED FOR ALL MySQL\n";
print "      SERVERS IN PRODUCTION USE!  PLEASE READ EACH STEP CAREFULLY!\n\n";

#-----------------------------------------------------------------------------
# get the current root password
#-----------------------------------------------------------------------------

print "In order to log into MySQL to secure it, we'll need the current\n";
print "password for the root user.  If you've just installed MySQL, and\n";
print "you haven't set the root password yet, the password will be blank,\n";
print "so you should just press enter here.\n\n";

print "Enter the current password for root: ";
my $password = <STDIN>;
chomp $password;
print "\n";

my $conn = Mysql->connect("localhost", "mysql", "root", $password)
  || die "Unable to connect to MySQL.";

print "OK, successfully used the password, moving on...\n\n";
 
#-----------------------------------------------------------------------------
# set the root password
#-----------------------------------------------------------------------------

unless ($password)
{
  print "Setting the root password ensures that no one can log into MySQL\n";
  print "using the root user without the proper authorization.\n\n";
    
  print "Set root password (Y/N)? ";
  my $reply = <STDIN>;
  chomp $reply;
  print "\n";
  
  if ($reply =~ /Y/i) 
  {
    print "New password for root: ";
    my $pass1 = <STDIN>;
    chomp $pass1;
    print "\n";
      
    print "Re-enter new password for root: ";
    my $pass2 = <STDIN>;
    chomp $pass2;
    print "\n";
      
    unless ($pass1 eq $pass2) { die "Sorry, the passwords do not match."; }
    
    unless ($pass1) { die "Sorry, you can't use an empty password here."; }
    
    $conn->query("SET PASSWORD FOR root\@localhost=PASSWORD('$pass1')")
      || die "Unable to set password.";
  
    print "OK, successfully set the password, moving on...\n\n";
  }
  else
  {
    print "WARNING, the password is not set, moving on...\n\n";
  }
}

#-----------------------------------------------------------------------------
# remove anonymous users
#-----------------------------------------------------------------------------

print "By default, a MySQL installation has anonymous users, allowing anyone\n";
print "to log into MySQL without having to have a user account created for\n";
print "them.  This is intended only for testing, and to make the installation\n";
print "go a bit smoother.  You should remove them before moving into a\n";
print "production environment.\n\n";

print "Remove anonymous users (Y/N)? ";
my $reply = <STDIN>;
chomp $reply;
print "\n";

if ($reply =~ /Y/i) 
{
  $conn->query("DELETE FROM mysql.user WHERE user=''")
    || die "Unable to remove anonymous users.";
  
  print "OK, successfully removed anonymous users, moving on...\n\n";
}
else
{
  print "WARNING, the anonymous users have not been removed, moving on...\n\n";
}

#-----------------------------------------------------------------------------
# disallow remote root login
#-----------------------------------------------------------------------------

print "Normally, root should only be allowed to connect from 'localhost'.  This\n";
print "ensures that someone cannot guess at the root password from the network.\n\n";

print "Disallow remote root login (Y/N)? ";
my $reply = <STDIN>;
chomp $reply;
print "\n";

if ($reply =~ /Y/i) 
{
  $conn->query("DELETE FROM mysql.user WHERE user='root' AND host!='localhost'")
    || die "Unable to disallow remote root login.";
  
  print "OK, successfully disallowed remote root login, moving on...\n\n";
}
else
{
  print "WARNING, remote root login has not been disallowed, moving on...\n\n";
}

#-----------------------------------------------------------------------------
# remove test database
#-----------------------------------------------------------------------------

print "By default, MySQL comes with a database named 'test' that anyone can\n";
print "access.  This is intended only for testing, and should be removed\n";
print "before moving into a production environment.\n\n";

print "Remove the test database (Y/N)? ";
my $reply = <STDIN>;
chomp $reply;
print "\n";

if ($reply =~ /Y/i) 
{
  $conn->query("DROP DATABASE IF EXISTS test")
    || die "Unable to remove test database.";
  
  $conn->query("DELETE FROM mysql.db WHERE db='test' OR db='test\\_%'")
    || die "Unable to remove access to the test database.";
  
  print "OK, successfully removed the test database, moving on...\n\n";
}
else
{
  print "WARNING, the test database has not been removed, moving on...\n\n";
}

#-----------------------------------------------------------------------------
# reload privilege tables
#-----------------------------------------------------------------------------

print "Reloading the privilege tables will ensure that all changes made so far\n";
print "will take effect immediately.\n\n";

print "Reload privilege tables (Y/N)? ";
my $reply = <STDIN>;
chomp $reply;
print "\n";

if ($reply =~ /Y/i) 
{
  $conn->query("FLUSH PRIVILEGES")
    || die "Unable to reload privilege tables.";
  
  print "OK, successfully reloaded privilege tables, moving on...\n\n";
}
else
{
  print "WARNING, the privilege tables have not been reloaded, moving on...\n\n";
}

#-----------------------------------------------------------------------------
# done
#-----------------------------------------------------------------------------

print "\n\nAll done!  If you've completed all of the above steps, your MySQL\n";
print "installation should now be secure.\n\n";

print "Thanks for using MySQL!\n\n";

