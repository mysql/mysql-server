#!/bin/sh

echo "This scripts updates the mysql.user, mysql.db, mysql.host and the"
echo "mysql.func table to MySQL 3.22.14 and above."
echo ""
echo "This is needed if you want to use the new GRANT functions or"
echo "want to use the more secure passwords."
echo ""
echo "If you get Access denied errors, you should run this script again"
echo "and give the MySQL root user password as a argument!"

root_password="$1"
host="localhost"

# Fix old password format, add File_priv and func table
echo ""
echo "If your tables are already up to date or partially up to date you will"
echo "get some warnings about 'Duplicated column name' or"
echo "'Table 'func' already exists'. You can safely ignore these!"

@bindir@/mysql -f --user=root --password="$root_password" --host="$host" mysql <<END_OF_DATA
alter table user change password password char(16) NOT NULL;
alter table user add File_priv enum('N','Y') NOT NULL;
CREATE TABLE func (
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
@bindir@/mysql --user=root --password="$root_password" --host="$host" mysql <<END_OF_DATA
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
  @bindir@/mysql --user=root --password="$root_password" --host="$host" mysql <<END_OF_DATA
  UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
END_OF_DATA
  echo ""
fi

#
# Create tables_priv and columns_priv if they don't exists
#

echo "Creating the new table and column privilege tables"

@bindir@/mysql -f --user=root --password="$root_password"  --host="$host" mysql <<END_OF_DATA
CREATE TABLE tables_priv (
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
CREATE TABLE columns_priv (
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
echo "You can ignore any errors from this"

@bindir@/mysql -f --user=root --password="$root_password"  --host="$host" mysql <<END_OF_DATA
ALTER TABLE columns_priv change Type Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL;
END_OF_DATA

#
# Add the new 'type' column to the func table.
#

echo "Fixing the func table"
echo "You can ignore any Duplicate column errors"

@bindir@/mysql --user=root --password=$root_password mysql <<EOF
alter table func add type enum ('function','aggregate') NOT NULL;
EOF
