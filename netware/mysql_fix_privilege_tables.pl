#-----------------------------------------------------------------------------
# Copyright (C) 2002 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
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

print "MySQL Fix Privilege Tables Script\n\n";

print "NOTE: This script updates your privilege tables to the lastest\n";
print "      specifications!\n\n";

#-----------------------------------------------------------------------------
# get the current root password
#-----------------------------------------------------------------------------

print "In order to log into MySQL to update it, we'll need the current\n";
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
# MySQL 4.0.2
#-----------------------------------------------------------------------------

print "Adding new fields used by MySQL 4.0.2 to the privilege tables...\n";
print "NOTE: You can ignore any Duplicate column errors.\n";
$conn->query(" \
ALTER TABLE user \
ADD Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER alter_priv, \
ADD Super_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_db_priv, \
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Super_priv, \
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_tmp_table_priv, \
ADD Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv, \
ADD Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Execute_priv, \
ADD Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Repl_slave_priv; \
") && $conn->query(" \
UPDATE user SET show_db_priv=select_priv, super_priv=process_priv, execute_priv=process_priv, create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=file_priv where user<>''; \
");

#-----------------------------------------------------------------------------
# MySQL 4.0 Limitations
#-----------------------------------------------------------------------------

print "Adding new fields used by MySQL 4.0 security limitations...\n";

$conn->query(" \
ALTER TABLE user \
ADD max_questions int(11) NOT NULL AFTER x509_subject, \
ADD max_updates   int(11) unsigned NOT NULL AFTER max_questions, \
ADD max_connections int(11) unsigned NOT NULL AFTER max_updates; \
");

#-----------------------------------------------------------------------------
# MySQL 4.0 DB and Host privs
#-----------------------------------------------------------------------------

print "Adding new fields used by MySQL 4.0 locking and temporary table security...\n";

$conn->query(" \
ALTER TABLE db \
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL, \
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL; \
");

$conn->query(" \
ALTER TABLE host \
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL, \
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL; \
");

#-----------------------------------------------------------------------------
# done
#-----------------------------------------------------------------------------

print "\n\nAll done!\n\n";

print "Thanks for using MySQL!\n\n";

