-- This scripts updates the mysql.user, mysql.db, mysql.host and the
-- mysql.func tables to MySQL 3.22.14 and above.

-- This is needed if you want to use the new GRANT functions,
-- CREATE AGGREGATE FUNCTION or want to use the more secure passwords in 3.23

-- If you get 'Access denied' errors, you should run this script again
-- and give the MySQL root user password as an argument!


-- Converting all privilege tables to MyISAM format
ALTER TABLE user type=MyISAM;
ALTER TABLE db type=MyISAM;
ALTER TABLE host type=MyISAM;
ALTER TABLE func type=MyISAM;
ALTER TABLE columns_priv type=MyISAM;
ALTER TABLE tables_priv type=MyISAM;


--  Fix old password format, add File_priv and func table

-- If your tables are already up to date or partially up to date you will
-- get some warnings about 'Duplicated column name'. You can safely ignore these!

alter table user change password password char(16) NOT NULL;
alter table user add File_priv enum('N','Y') NOT NULL;
CREATE TABLE if not exists func (
  name char(64) DEFAULT '' NOT NULL,
  ret tinyint(1) DEFAULT '0' NOT NULL,
  dl char(128) DEFAULT '' NOT NULL,
  type enum ('function','aggregate') NOT NULL,
  PRIMARY KEY (name)
);

--  Add the new grant colums

-- Creating Grant Alter and Index privileges if they don't exists
-- You can ignore any Duplicate column errors
alter table user add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
alter table host add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
alter table db add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;

--  If the new grant columns didn't exists, copy File -> Grant
--  and Create -> Alter, Index, References

-- Setting default privileges for the new grant, index and alter privileges
  UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;
  UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv;

--
--  The second alter changes ssl_type to new 4.0.2 format

-- Adding columns needed by GRANT .. REQUIRE (openssl)"
-- You can ignore any Duplicate column errors"
ALTER TABLE user
ADD ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL,
ADD ssl_cipher BLOB NOT NULL,
ADD x509_issuer BLOB NOT NULL,
ADD x509_subject BLOB NOT NULL;
ALTER TABLE user MODIFY ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL;

--
--  Create tables_priv and columns_priv if they don't exists
--

-- Creating the new table and column privilege tables"

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

--
--  Name change of Type -> Column_priv from MySQL 3.22.12
--

-- Changing name of columns_priv.Type -> columns_priv.Column_priv
-- You can ignore any Unknown column errors from this


ALTER TABLE columns_priv change Type Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL;



--
--  Add the new 'type' column to the func table.
--

-- Fixing the func table
-- You can ignore any Duplicate column errors

alter table func add type enum ('function','aggregate') NOT NULL;


--
--  Change the user,db and host tables to MySQL 4.0 format
--

-- Adding new fields used by MySQL 4.0.2 to the privilege tables
-- You can ignore any Duplicate column errors


alter table user
add Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER alter_priv,
add Super_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_db_priv,
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Super_priv,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_tmp_table_priv,
add Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv,
add Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Execute_priv,
add Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Repl_slave_priv;


-- Convert privileges so that users have similar privileges as before

-- Updating new privileges in MySQL 4.0.2 from old ones

  update user set show_db_priv= select_priv, super_priv=process_priv, execute_priv=process_priv, create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=file_priv where user<>"";


--  Add fields that can be used to limit number of questions and connections
--  for some users.


alter table user
add max_questions int(11) NOT NULL AFTER x509_subject,
add max_updates   int(11) unsigned NOT NULL AFTER max_questions,
add max_connections int(11) unsigned NOT NULL AFTER max_updates;


--
--  Add Create_tmp_table_priv and Lock_tables_priv to db and host
--


alter table db
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;
alter table host
add Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
add Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;

