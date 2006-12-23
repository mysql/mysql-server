#-----------------------------------------------------------------------------
# Copyright (C) 2002 MySQL AB
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

#use strict;
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

#-- Detect whether or not we had the Grant_priv column
print "Fixing privileges for old tables...\n";
$conn->query("SET \@hadGrantPriv:=0;");
$conn->query("SELECT \@hadGrantPriv:=1 FROM user WHERE Grant_priv LIKE '%';");

#--- Fix privileges for old tables
$conn->query("UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE \@hadGrantPriv = 0;");
$conn->query("UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE \@hadGrantPriv = 0;");
$conn->query("UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE \@hadGrantPriv = 0;");


# Detect whether we had Show_db_priv
$conn->query("SET \@hadShowDbPriv:=0;");
$conn->query("SELECT \@hadShowDbPriv:=1 FROM user WHERE Show_db_priv LIKE '%';");

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
UPDATE user SET show_db_priv=select_priv, super_priv=process_priv, execute_priv=process_priv, create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=file_priv where user<>''AND \@hadShowDbPriv = 0; \
");

#-- The above statement converts privileges so that users have similar privileges as before

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

#-- Change the password column to suite the new password hashing used
#-- in 4.1.1 onward
$conn->query("ALTER TABLE user change Password Password char(41) binary not null;");

#-- The second alter changes ssl_type to new 4.0.2 format
#-- Adding columns needed by GRANT .. REQUIRE (openssl)"
print "Adding new fields to use in ssl authentication...\n";

$conn->query(" \
ALTER TABLE user \
ADD ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL, \
ADD ssl_cipher BLOB NOT NULL, \
ADD x509_issuer BLOB NOT NULL, \
ADD x509_subject BLOB NOT NULL; \
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

#
# Change the Table_name column to be of char(64) which was char(60) by mistake till now.
#
$conn->query("alter table tables_priv change Table_name Table_name char(64) binary DEFAULT '' NOT NULL;");


#
# Create some possible missing tables
#
print "Adding online help tables...\n";

$conn->query(" \
CREATE TABLE IF NOT EXISTS help_topic ( \
help_topic_id int unsigned not null, \
name varchar(64) not null, \
help_category_id smallint unsigned not null, \
description text not null, \
example text not null, \
url varchar(128) not null, \
primary key (help_topic_id), unique index (name) \
) comment='help topics'; \
");

$conn->query(" \
CREATE TABLE IF NOT EXISTS help_category ( \
help_category_id smallint unsigned not null, \
name varchar(64) not null, \
parent_category_id smallint unsigned null, \
url varchar(128) not null, \
primary key (help_category_id), \
unique index (name) \
) comment='help categories'; \
");

$conn->query(" \
CREATE TABLE IF NOT EXISTS help_relation ( \
help_topic_id int unsigned not null references help_topic, \
help_keyword_id  int unsigned not null references help_keyword, \
primary key (help_keyword_id, help_topic_id) \
) comment='keyword-topic relation'; \
");

$conn->query(" \
CREATE TABLE IF NOT EXISTS help_keyword ( \
help_keyword_id int unsigned not null, \
name varchar(64) not null, \
primary key (help_keyword_id), \
unique index (name) \
) comment='help keywords'; \
");


#
# Filling the help tables with contents.
#
print "Filling online help tables with contents...\n";
# Generate the path for "fill_help_tables.sql" file which is in different folder. 
$fill_help_table=$0;
$fill_help_table =~ s/scripts[\\\/]mysql_fix_privilege_tables.pl/share\\fill_help_tables.sql/;

#read all content from the sql file which contains recordsfor help tables.
open(fileIN,$fill_help_table) or die("Cannot open $fill_help_table: $!");
@logData = <fileIN>;
close(fileIN);
foreach $line (@logData) {
# if the line is not empty, insert a record in the table.
    if( ! ($line =~ /^\s*$/) ) {
        $conn->query("$line");
    }
}

#-----------------------------------------------------------------------------
# done
#-----------------------------------------------------------------------------

print "\n\nAll done!\n\n";

print "Thanks for using MySQL!\n\n";
