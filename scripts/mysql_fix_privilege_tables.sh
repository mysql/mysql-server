#!/bin/sh

root_password="$1"
host="localhost"
user="root"

if test -z "$1" ; then
  cmd="@bindir@/mysql -f --user=$user --host=$host mysql"
else
  root_password="$1"
  cmd="@bindir@/mysql -f --user=$user --password=$root_password --host=$host mysql"
fi

# Debian addition
if [ "$1" = "--sql-only" ]; then
  root_password=""
  cmd="/usr/share/mysql/echo_stderr"
fi

echo "This scripts updates the mysql.user, mysql.db, mysql.host and the"
echo "mysql.func tables to MySQL 3.22.14 and above."
echo ""
echo "This is needed if you want to use the new GRANT functions,"
echo "CREATE AGGREGATE FUNCTION or want to use the more secure passwords in 3.23"
echo ""
echo "If you get 'Access denied' errors, you should run this script again"
echo "and give the MySQL root user password as an argument!"

echo "Converting all privilege tables to MyISAM format"
$cmd <<END_OF_DATA
ALTER TABLE user type=MyISAM;
ALTER TABLE db type=MyISAM;
ALTER TABLE host type=MyISAM;
ALTER TABLE func type=MyISAM;
ALTER TABLE columns_priv type=MyISAM;
ALTER TABLE tables_priv type=MyISAM;
END_OF_DATA


# Fix old password format, add File_priv and func table
echo ""
echo "If your tables are already up to date or partially up to date you will"
echo "get some warnings about 'Duplicated column name'. You can safely ignore these!"

$cmd <<END_OF_DATA
alter table user change password password char(16) NOT NULL;
alter table user add File_priv enum('N','Y') NOT NULL;
CREATE TABLE if not exists func (
  name char(64) DEFAULT '' NOT NULL,
  ret tinyint(1) DEFAULT '0' NOT NULL,
  dl char(128) DEFAULT '' NOT NULL,
  type enum ('function','aggregate') NOT NULL,
  PRIMARY KEY (name)
);
END_OF_DATA
echo ""

# Add the new grant colums

echo "Creating Grant Alter and Index privileges if they don't exists"
echo "You can ignore any Duplicate column errors"
$cmd <<END_OF_DATA
alter table user add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
alter table host add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
alter table db add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
END_OF_DATA
res=$?
echo ""

# If the new grant columns didn't exists, copy File -> Grant
# and Create -> Alter, Index, References

if test $res = 0
then
  echo "Setting default privileges for the new grant, index and alter privileges"
  $cmd <<END_OF_DATA
  UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
END_OF_DATA
  echo ""
fi

#
# The second alter changes ssl_type to new 4.0.2 format

echo "Adding columns needed by GRANT .. REQUIRE (openssl)"
echo "You can ignore any Duplicate column errors"
$cmd <<END_OF_DATA
ALTER TABLE user
ADD ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL,
ADD ssl_cipher BLOB NOT NULL,
ADD x509_issuer BLOB NOT NULL,
ADD x509_subject BLOB NOT NULL;
ALTER TABLE user MODIFY ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL;
END_OF_DATA
echo ""

#
# Create tables_priv and columns_priv if they don't exists
#

echo "Creating the new table and column privilege tables"

$cmd <<END_OF_DATA
CREATE TABLE IF NOT EXISTS tables_priv (
  Host char(60) DEFAULT '' NOT NULL,
  Db char(60) DEFAULT '' NOT NULL,
  User char(16) DEFAULT '' NOT NULL,
  Table_name char(60) DEFAULT '' NOT NULL,
  Grantor char(77) DEFAULT '' NOT NULL,
  Timestamp timestamp(14),
  Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter') DEFAULT '' NOT NULL,
  Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,
  PRIMARY KEY (Host,Db,User,Table_name)
);
CREATE TABLE IF NOT EXISTS columns_priv (
  Host char(60) DEFAULT '' NOT NULL,
  Db char(60) DEFAULT '' NOT NULL,
  User char(16) DEFAULT '' NOT NULL,
  Table_name char(60) DEFAULT '' NOT NULL,
  Column_name char(59) DEFAULT '' NOT NULL,
  Timestamp timestamp(14),
  Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,
  PRIMARY KEY (Host,Db,User,Table_name,Column_name)
);
END_OF_DATA

#
# Name change of Type -> Column_priv from MySQL 3.22.12
#

echo "Changing name of columns_priv.Type -> columns_priv.Column_priv"
echo "You can ignore any Unknown column errors from this"

$cmd <<END_OF_DATA
ALTER TABLE columns_priv change Type Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL;
END_OF_DATA
echo ""

#
# Add the new 'type' column to the func table.
#

echo "Fixing the func table"
echo "You can ignore any Duplicate column errors"

$cmd <<EOF
alter table func add type enum ('function','aggregate') NOT NULL;
EOF
echo ""

#
# Change the user,db and host tables to MySQL 4.0 format
#

echo "Adding new fields used by MySQL 4.0.2 to the privilege tables"
echo "You can ignore any Duplicate column errors"

$cmd <<END_OF_DATA
alter table user
add Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER alter_priv,
add Super_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_db_priv,
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Super_priv,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_tmp_table_priv,
add Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv,
add Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Execute_priv,
add Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Repl_slave_priv;
END_OF_DATA

if test $? -eq "0"
then
  # Convert privileges so that users have similar privileges as before
  echo ""
  echo "Updating new privileges in MySQL 4.0.2 from old ones"
  $cmd <<END_OF_DATA
  update user set show_db_priv= select_priv, super_priv=process_priv, execute_priv=process_priv, create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=file_priv where user<>"";
END_OF_DATA
  echo ""
fi

# Add fields that can be used to limit number of questions and connections
# for some users.

$cmd <<END_OF_DATA
alter table user
add max_questions int(11) NOT NULL AFTER x509_subject,
add max_updates   int(11) unsigned NOT NULL AFTER max_questions,
add max_connections int(11) unsigned NOT NULL AFTER max_updates;
END_OF_DATA

#
# Add Create_tmp_table_priv and Lock_tables_priv to db and host
#

$cmd <<END_OF_DATA
alter table db
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;
alter table host
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;
END_OF_DATA
