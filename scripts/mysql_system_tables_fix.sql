-- Copyright (C) 2003, 2013 Oracle and/or its affiliates.
-- Copyright (C) 2010, 2014 SkySQL Ab.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This part converts any old privilege tables to privilege tables suitable
# for current version of MySQL

# You can safely ignore all 'Duplicate column' and 'Unknown column' errors
# because these just mean that your tables are already up to date.
# This script is safe to run even if your tables are already up to date!

# To facilitate custom changes to system tables that might have been done 
# (as per MDEV-4332, MDEV-6068 and https://mariadb.com/kb/en/create-user/#user-names),
# we need to preserve non-default column length for user name fields.
# These variables will be further used to alter system tables.
# We presume that the set of ALTERs, if it was run, was run in full,
# so either all columns were modified, or none was. 

SELECT character_maximum_length, IF(character_maximum_length = 16,77,141) 
  FROM INFORMATION_SCHEMA.COLUMNS
  WHERE table_schema = 'mysql' AND table_name = 'user' AND column_name = 'user'
  INTO @user_name_length, @definer_name_length;

set sql_mode='';
set storage_engine=MyISAM;

ALTER TABLE user add File_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL;

# Detect whether or not we had the Grant_priv column
SET @hadGrantPriv:=0;
SELECT @hadGrantPriv:=1 FROM user WHERE Grant_priv LIKE '%';

ALTER TABLE user add Grant_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add References_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Index_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Alter_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL;
ALTER TABLE host add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Index_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Alter_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL;
ALTER TABLE db add Grant_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add References_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Index_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL,add Alter_priv enum('N','Y') COLLATE utf8_general_ci NOT NULL;

# Fix privileges for old tables
UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;
UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;
UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;

#
# The second alter changes ssl_type to new 4.0.2 format
# Adding columns needed by GRANT .. REQUIRE (openssl)

ALTER TABLE user
ADD ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci NOT NULL,
ADD ssl_cipher BLOB NOT NULL,
ADD x509_issuer BLOB NOT NULL,
ADD x509_subject BLOB NOT NULL;
ALTER TABLE user MODIFY ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL;

#
# tables_priv
#
ALTER TABLE tables_priv
  ADD KEY Grantor (Grantor);

SET @alter_statement = CONCAT("
ALTER TABLE tables_priv
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(", @user_name_length, ") NOT NULL default '',
  MODIFY Table_name char(64) NOT NULL default '',
  MODIFY Grantor char(", @definer_name_length, ") NOT NULL default '',
  ENGINE=MyISAM,
  CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin
");
PREPARE alter_stmt FROM @alter_statement;
EXECUTE alter_stmt;

ALTER TABLE tables_priv
  MODIFY Column_priv set('Select','Insert','Update','References')
    COLLATE utf8_general_ci DEFAULT '' NOT NULL,
  MODIFY Table_priv set('Select','Insert','Update','Delete','Create',
                        'Drop','Grant','References','Index','Alter',
                        'Create View','Show view','Trigger')
    COLLATE utf8_general_ci DEFAULT '' NOT NULL,
  COMMENT='Table privileges';

#
# columns_priv
#
#
# Name change of Type -> Column_priv from MySQL 3.22.12
#
ALTER TABLE columns_priv
  CHANGE Type Column_priv set('Select','Insert','Update','References')
    COLLATE utf8_general_ci DEFAULT '' NOT NULL;

SET @alter_statement = CONCAT("
ALTER TABLE columns_priv
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(", @user_name_length, ") NOT NULL default '',
  MODIFY Table_name char(64) NOT NULL default '',
  MODIFY Column_name char(64) NOT NULL default '',
  ENGINE=MyISAM,
  CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin,
  COMMENT='Column privileges'
");
PREPARE alter_stmt FROM @alter_statement;
EXECUTE alter_stmt;

ALTER TABLE columns_priv
  MODIFY Column_priv set('Select','Insert','Update','References')
    COLLATE utf8_general_ci DEFAULT '' NOT NULL;

#
#  Add the new 'type' column to the func table.
#

ALTER TABLE func add type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL;

#
#  Change the user,db and host tables to current format
#

# Detect whether we had Show_db_priv
SET @hadShowDbPriv:=0;
SELECT @hadShowDbPriv:=1 FROM user WHERE Show_db_priv LIKE '%';

ALTER TABLE user
ADD Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_priv,
ADD Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_db_priv,
ADD Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Super_priv,
ADD Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_tmp_table_priv,
ADD Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Lock_tables_priv,
ADD Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Execute_priv,
ADD Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Repl_slave_priv;

# Convert privileges so that users have similar privileges as before

UPDATE user SET Show_db_priv= Select_priv, Super_priv=Process_priv, Execute_priv=Process_priv, Create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=File_priv where user<>"" AND @hadShowDbPriv = 0;


#  Add fields that can be used to limit number of questions and connections
#  for some users.

ALTER TABLE user
ADD max_questions int(11) NOT NULL DEFAULT 0 AFTER x509_subject,
ADD max_updates   int(11) unsigned NOT NULL DEFAULT 0 AFTER max_questions,
ADD max_connections int(11) unsigned NOT NULL DEFAULT 0 AFTER max_updates;


#
#  Add Create_tmp_table_priv and Lock_tables_priv to db and host
#

ALTER TABLE db
ADD Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;
ALTER TABLE host
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;

alter table user change max_questions max_questions int(11) unsigned DEFAULT 0  NOT NULL;


alter table db comment='Database privileges';
alter table host comment='Host privileges;  Merged with database privileges';
alter table user comment='Users and global privileges';
alter table func comment='User defined functions';

# Convert all tables to UTF-8 with binary collation
# and reset all char columns to correct width

SET @alter_statement = CONCAT("
ALTER TABLE user
  MODIFY Host char(60) NOT NULL default '',
  MODIFY User char(", @user_name_length, ") NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin
");
PREPARE alter_stmt FROM @alter_statement;
EXECUTE alter_stmt;

ALTER TABLE user
  MODIFY Password char(41) character set latin1 collate latin1_bin NOT NULL default '',
  MODIFY Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL;

SET @alter_statement = CONCAT("
ALTER TABLE db
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(", @user_name_length, ") NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin
");
PREPARE alter_stmt FROM @alter_statement;
EXECUTE alter_stmt;

ALTER TABLE db
  MODIFY  Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY  Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;

ALTER TABLE host
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE host
  MODIFY Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
  MODIFY Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;

ALTER TABLE func
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE func
  MODIFY type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL;

#
# Modify log tables.
#

SET @old_log_state = @@global.general_log;
SET GLOBAL general_log = 'OFF';
ALTER TABLE general_log
  MODIFY event_time TIMESTAMP(6) NOT NULL,
  MODIFY user_host MEDIUMTEXT NOT NULL,
  MODIFY thread_id INTEGER NOT NULL,
  MODIFY server_id INTEGER UNSIGNED NOT NULL,
  MODIFY command_type VARCHAR(64) NOT NULL,
  MODIFY argument MEDIUMTEXT NOT NULL;
SET GLOBAL general_log = @old_log_state;

SET @old_log_state = @@global.slow_query_log;
SET GLOBAL slow_query_log = 'OFF';
ALTER TABLE slow_log
  MODIFY start_time TIMESTAMP(6) NOT NULL,
  MODIFY user_host MEDIUMTEXT NOT NULL,
  MODIFY query_time TIME(6) NOT NULL,
  MODIFY lock_time TIME(6) NOT NULL,
  MODIFY rows_sent INTEGER NOT NULL,
  MODIFY rows_examined INTEGER NOT NULL,
  MODIFY db VARCHAR(512) NOT NULL,
  MODIFY last_insert_id INTEGER NOT NULL,
  MODIFY insert_id INTEGER NOT NULL,
  MODIFY server_id INTEGER UNSIGNED NOT NULL,
  MODIFY sql_text MEDIUMTEXT NOT NULL;
SET GLOBAL slow_query_log = @old_log_state;

ALTER TABLE plugin
  MODIFY name varchar(64) COLLATE utf8_general_ci NOT NULL DEFAULT '',
  MODIFY dl varchar(128) COLLATE utf8_general_ci NOT NULL DEFAULT '',
  CONVERT TO CHARACTER SET utf8 COLLATE utf8_general_ci;

#
# Detect whether we had Create_view_priv
#
SET @hadCreateViewPriv:=0;
SELECT @hadCreateViewPriv:=1 FROM user WHERE Create_view_priv LIKE '%';

#
# Create VIEWs privileges (v5.0)
#
ALTER TABLE db ADD Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;
ALTER TABLE db MODIFY Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;

ALTER TABLE host ADD Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;
ALTER TABLE host MODIFY Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;

ALTER TABLE user ADD Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Repl_client_priv;
ALTER TABLE user MODIFY Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Repl_client_priv;

#
# Show VIEWs privileges (v5.0)
#
ALTER TABLE db ADD Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;
ALTER TABLE db MODIFY Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;

ALTER TABLE host ADD Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;
ALTER TABLE host MODIFY Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;

ALTER TABLE user ADD Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;
ALTER TABLE user MODIFY Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_view_priv;

#
# Assign create/show view privileges to people who have create provileges
#
UPDATE user SET Create_view_priv=Create_priv, Show_view_priv=Create_priv where user<>"" AND @hadCreateViewPriv = 0;

#
#
#
SET @hadCreateRoutinePriv:=0;
SELECT @hadCreateRoutinePriv:=1 FROM user WHERE Create_routine_priv LIKE '%';

#
# Create PROCEDUREs privileges (v5.0)
#
ALTER TABLE db ADD Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;
ALTER TABLE db MODIFY Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;

ALTER TABLE host ADD Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;
ALTER TABLE host MODIFY Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;

ALTER TABLE user ADD Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;
ALTER TABLE user MODIFY Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Show_view_priv;

#
# Alter PROCEDUREs privileges (v5.0)
#
ALTER TABLE db ADD Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;
ALTER TABLE db MODIFY Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;

ALTER TABLE host ADD Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;
ALTER TABLE host MODIFY Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;

ALTER TABLE user ADD Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;
ALTER TABLE user MODIFY Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Create_routine_priv;

ALTER TABLE db ADD Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;
ALTER TABLE db MODIFY Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;

ALTER TABLE host ADD Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;
ALTER TABLE host MODIFY Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;

#
# Assign create/alter routine privileges to people who have create privileges
#
UPDATE user SET Create_routine_priv=Create_priv, Alter_routine_priv=Alter_priv where user<>"" AND @hadCreateRoutinePriv = 0;
UPDATE db SET Create_routine_priv=Create_priv, Alter_routine_priv=Alter_priv, Execute_priv=Select_priv where user<>"" AND @hadCreateRoutinePriv = 0;
UPDATE host SET Create_routine_priv=Create_priv, Alter_routine_priv=Alter_priv, Execute_priv=Select_priv where @hadCreateRoutinePriv = 0;

#
# Add max_user_connections resource limit
# this is signed in MariaDB so that if one sets it's to -1 then the user
# can't connect anymore.
#
ALTER TABLE user ADD max_user_connections int(11) DEFAULT '0' NOT NULL AFTER max_connections;
ALTER TABLE user MODIFY max_user_connections int(11) DEFAULT '0' NOT NULL AFTER max_connections;

#
# user.Create_user_priv
#

SET @hadCreateUserPriv:=0;
SELECT @hadCreateUserPriv:=1 FROM user WHERE Create_user_priv LIKE '%';

ALTER TABLE user ADD Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;
ALTER TABLE user MODIFY Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;
UPDATE user LEFT JOIN db USING (Host,User) SET Create_user_priv='Y'
  WHERE @hadCreateUserPriv = 0 AND
        (user.Grant_priv = 'Y' OR db.Grant_priv = 'Y');

#
# procs_priv
#

ALTER TABLE procs_priv
  ENGINE=MyISAM,
  CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;

ALTER TABLE procs_priv
  MODIFY Proc_priv set('Execute','Alter Routine','Grant')
    COLLATE utf8_general_ci DEFAULT '' NOT NULL;

ALTER IGNORE TABLE procs_priv
  MODIFY Routine_name char(64)
    COLLATE utf8_general_ci DEFAULT '' NOT NULL;

ALTER TABLE procs_priv
  ADD Routine_type enum('FUNCTION','PROCEDURE')
    COLLATE utf8_general_ci NOT NULL AFTER Routine_name;

ALTER TABLE procs_priv
  MODIFY Timestamp timestamp AFTER Proc_priv;

#
# proc
#

# Correct the name fields to not binary, and expand sql_data_access
ALTER TABLE proc MODIFY name char(64) DEFAULT '' NOT NULL,
                 MODIFY specific_name char(64) DEFAULT '' NOT NULL,
                 MODIFY sql_data_access
                        enum('CONTAINS_SQL',
                             'NO_SQL',
                             'READS_SQL_DATA',
                             'MODIFIES_SQL_DATA'
                            ) DEFAULT 'CONTAINS_SQL' NOT NULL,
                 MODIFY body longblob NOT NULL,
                 MODIFY returns longblob NOT NULL,
                 MODIFY sql_mode
                        set('REAL_AS_FLOAT',
                            'PIPES_AS_CONCAT',
                            'ANSI_QUOTES',
                            'IGNORE_SPACE',
                            'IGNORE_BAD_TABLE_OPTIONS',
                            'ONLY_FULL_GROUP_BY',
                            'NO_UNSIGNED_SUBTRACTION',
                            'NO_DIR_IN_CREATE',
                            'POSTGRESQL',
                            'ORACLE',
                            'MSSQL',
                            'DB2',
                            'MAXDB',
                            'NO_KEY_OPTIONS',
                            'NO_TABLE_OPTIONS',
                            'NO_FIELD_OPTIONS',
                            'MYSQL323',
                            'MYSQL40',
                            'ANSI',
                            'NO_AUTO_VALUE_ON_ZERO',
                            'NO_BACKSLASH_ESCAPES',
                            'STRICT_TRANS_TABLES',
                            'STRICT_ALL_TABLES',
                            'NO_ZERO_IN_DATE',
                            'NO_ZERO_DATE',
                            'INVALID_DATES',
                            'ERROR_FOR_DIVISION_BY_ZERO',
                            'TRADITIONAL',
                            'NO_AUTO_CREATE_USER',
                            'HIGH_NOT_PRECEDENCE',
                            'NO_ENGINE_SUBSTITUTION',
                            'PAD_CHAR_TO_FULL_LENGTH'
                            ) DEFAULT '' NOT NULL,
                 DEFAULT CHARACTER SET utf8;

# Correct the character set and collation
ALTER TABLE proc CONVERT TO CHARACTER SET utf8;
# Reset some fields after the conversion
SET @alter_statement = CONCAT("
ALTER TABLE proc  MODIFY db
                         char(64) collate utf8_bin DEFAULT '' NOT NULL,
                  MODIFY definer
                         char(", @definer_name_length, ") collate utf8_bin DEFAULT '' NOT NULL,
                  MODIFY comment
                         char(64) collate utf8_bin DEFAULT '' NOT NULL
");
PREPARE alter_stmt FROM @alter_statement;
EXECUTE alter_stmt;

ALTER TABLE proc ADD character_set_client
                     char(32) collate utf8_bin DEFAULT NULL
                     AFTER comment;
ALTER TABLE proc MODIFY character_set_client
                        char(32) collate utf8_bin DEFAULT NULL;

SELECT CASE WHEN COUNT(*) > 0 THEN 
CONCAT ("WARNING: NULL values of the 'character_set_client' column ('mysql.proc' table) have been updated with a default value (", @@character_set_client, "). Please verify if necessary.")
ELSE NULL 
END 
AS value FROM proc WHERE character_set_client IS NULL;

UPDATE proc SET character_set_client = @@character_set_client 
                     WHERE character_set_client IS NULL;

ALTER TABLE proc ADD collation_connection
                     char(32) collate utf8_bin DEFAULT NULL
                     AFTER character_set_client;
ALTER TABLE proc MODIFY collation_connection
                        char(32) collate utf8_bin DEFAULT NULL;

SELECT CASE WHEN COUNT(*) > 0 THEN 
CONCAT ("WARNING: NULL values of the 'collation_connection' column ('mysql.proc' table) have been updated with a default value (", @@collation_connection, "). Please verify if necessary.")
ELSE NULL 
END 
AS value FROM proc WHERE collation_connection IS NULL;

UPDATE proc SET collation_connection = @@collation_connection
                     WHERE collation_connection IS NULL;

ALTER TABLE proc ADD db_collation
                     char(32) collate utf8_bin DEFAULT NULL
                     AFTER collation_connection;
ALTER TABLE proc MODIFY db_collation
                        char(32) collate utf8_bin DEFAULT NULL;

SELECT CASE WHEN COUNT(*) > 0 THEN 
CONCAT ("WARNING: NULL values of the 'db_collation' column ('mysql.proc' table) have been updated with default values. Please verify if necessary.")
ELSE NULL
END
AS value FROM proc WHERE db_collation IS NULL;

UPDATE proc AS p SET db_collation  = 
                     ( SELECT DEFAULT_COLLATION_NAME 
                       FROM INFORMATION_SCHEMA.SCHEMATA 
                       WHERE SCHEMA_NAME = p.db)
                     WHERE db_collation IS NULL;

ALTER TABLE proc ADD body_utf8 longblob DEFAULT NULL
                     AFTER db_collation;
ALTER TABLE proc MODIFY body_utf8 longblob DEFAULT NULL;

# Change comment from char(64) to text
ALTER TABLE proc MODIFY comment
                        text collate utf8_bin NOT NULL;

#
# EVENT privilege
#
SET @hadEventPriv := 0;
SELECT @hadEventPriv :=1 FROM user WHERE Event_priv LIKE '%';

ALTER TABLE user add Event_priv enum('N','Y') character set utf8 DEFAULT 'N' NOT NULL AFTER Create_user_priv;
ALTER TABLE user MODIFY Event_priv enum('N','Y') character set utf8 DEFAULT 'N' NOT NULL AFTER Create_user_priv;

UPDATE user SET Event_priv=Super_priv WHERE @hadEventPriv = 0;

ALTER TABLE db add Event_priv enum('N','Y') character set utf8 DEFAULT 'N' NOT NULL;
ALTER TABLE db MODIFY Event_priv enum('N','Y') character set utf8 DEFAULT 'N' NOT NULL;

#
# EVENT table
#
ALTER TABLE event DROP PRIMARY KEY;
ALTER TABLE event ADD PRIMARY KEY(db, name);
# Add sql_mode column just in case.
ALTER TABLE event ADD sql_mode set ('IGNORE_BAD_TABLE_OPTIONS') AFTER on_completion;
# Update list of sql_mode values.
ALTER TABLE event MODIFY sql_mode
                        set('REAL_AS_FLOAT',
                            'PIPES_AS_CONCAT',
                            'ANSI_QUOTES',
                            'IGNORE_SPACE',
                            'IGNORE_BAD_TABLE_OPTIONS',
                            'ONLY_FULL_GROUP_BY',
                            'NO_UNSIGNED_SUBTRACTION',
                            'NO_DIR_IN_CREATE',
                            'POSTGRESQL',
                            'ORACLE',
                            'MSSQL',
                            'DB2',
                            'MAXDB',
                            'NO_KEY_OPTIONS',
                            'NO_TABLE_OPTIONS',
                            'NO_FIELD_OPTIONS',
                            'MYSQL323',
                            'MYSQL40',
                            'ANSI',
                            'NO_AUTO_VALUE_ON_ZERO',
                            'NO_BACKSLASH_ESCAPES',
                            'STRICT_TRANS_TABLES',
                            'STRICT_ALL_TABLES',
                            'NO_ZERO_IN_DATE',
                            'NO_ZERO_DATE',
                            'INVALID_DATES',
                            'ERROR_FOR_DIVISION_BY_ZERO',
                            'TRADITIONAL',
                            'NO_AUTO_CREATE_USER',
                            'HIGH_NOT_PRECEDENCE',
                            'NO_ENGINE_SUBSTITUTION',
                            'PAD_CHAR_TO_FULL_LENGTH'
                            ) DEFAULT '' NOT NULL AFTER on_completion;
ALTER TABLE event MODIFY name char(64) CHARACTER SET utf8 NOT NULL default '';

ALTER TABLE event MODIFY COLUMN originator INT UNSIGNED NOT NULL;
ALTER TABLE event ADD COLUMN originator INT UNSIGNED NOT NULL AFTER comment;

ALTER TABLE event MODIFY COLUMN status ENUM('ENABLED','DISABLED','SLAVESIDE_DISABLED') NOT NULL default 'ENABLED';

ALTER TABLE event ADD COLUMN time_zone char(64) CHARACTER SET latin1
        NOT NULL DEFAULT 'SYSTEM' AFTER originator;

ALTER TABLE event ADD character_set_client
                      char(32) collate utf8_bin DEFAULT NULL
                      AFTER time_zone;
ALTER TABLE event MODIFY character_set_client
                         char(32) collate utf8_bin DEFAULT NULL;

ALTER TABLE event ADD collation_connection
                      char(32) collate utf8_bin DEFAULT NULL
                      AFTER character_set_client;
ALTER TABLE event MODIFY collation_connection
                         char(32) collate utf8_bin DEFAULT NULL;

ALTER TABLE event ADD db_collation
                      char(32) collate utf8_bin DEFAULT NULL
                      AFTER collation_connection;
ALTER TABLE event MODIFY db_collation
                         char(32) collate utf8_bin DEFAULT NULL;

ALTER TABLE event ADD body_utf8 longblob DEFAULT NULL
                      AFTER db_collation;
ALTER TABLE event MODIFY body_utf8 longblob DEFAULT NULL;


#
# TRIGGER privilege
#

SET @hadTriggerPriv := 0;
SELECT @hadTriggerPriv :=1 FROM user WHERE Trigger_priv LIKE '%';

ALTER TABLE user ADD Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Event_priv;
ALTER TABLE user MODIFY Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Event_priv;

ALTER TABLE host ADD Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;
ALTER TABLE host MODIFY Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;

ALTER TABLE db ADD Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;
ALTER TABLE db MODIFY Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL;

UPDATE user SET Trigger_priv=Super_priv WHERE @hadTriggerPriv = 0;

#
# user.Create_tablespace_priv
#

SET @hadCreateTablespacePriv := 0;
SELECT @hadCreateTablespacePriv :=1 FROM user WHERE Create_tablespace_priv LIKE '%';

ALTER TABLE user ADD Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Trigger_priv;
ALTER TABLE user MODIFY Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL AFTER Trigger_priv;

UPDATE user SET Create_tablespace_priv = Super_priv WHERE @hadCreateTablespacePriv = 0;

ALTER TABLE user ADD plugin char(64) DEFAULT '',  ADD authentication_string TEXT;
ALTER TABLE user MODIFY plugin char(64) CHARACTER SET latin1 DEFAULT '' NOT NULL;
ALTER TABLE user MODIFY authentication_string TEXT NOT NULL;

-- Need to pre-fill mysql.proxies_priv with access for root even when upgrading from
-- older versions

CREATE TEMPORARY TABLE tmp_proxies_priv LIKE proxies_priv;
INSERT INTO tmp_proxies_priv VALUES ('localhost', 'root', '', '', TRUE, '', now());
INSERT INTO proxies_priv SELECT * FROM tmp_proxies_priv WHERE @had_proxies_priv_table=0;
DROP TABLE tmp_proxies_priv;

-- Checking for any duplicate hostname and username combination are exists.
-- If exits we will throw error.
DROP PROCEDURE IF EXISTS mysql.count_duplicate_host_names;
DELIMITER //
CREATE PROCEDURE mysql.count_duplicate_host_names()
BEGIN
  SET @duplicate_hosts=(SELECT count(*) FROM mysql.user GROUP BY user, lower(host) HAVING count(*) > 1 LIMIT 1);
  IF @duplicate_hosts > 1 THEN
    SIGNAL SQLSTATE '45000'  SET MESSAGE_TEXT = 'Multiple accounts exist for @user_name, @host_name that differ only in Host lettercase; remove all except one of them';
  END IF;
END //
DELIMITER ;
CALL mysql.count_duplicate_host_names();
-- Get warnings (if any)
SHOW WARNINGS;
DROP PROCEDURE mysql.count_duplicate_host_names;

# Convering the host name to lower case for existing users
UPDATE user SET host=LOWER( host ) WHERE LOWER( host ) <> host;

# Activate the new, possible modified privilege tables
# This should not be needed, but gives us some extra testing that the above
# changes was correct

flush privileges;
DEALLOCATE PREPARE alter_stmt;

