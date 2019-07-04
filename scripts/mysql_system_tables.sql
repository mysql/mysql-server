-- Copyright (c) 2007, 2019, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

--
-- The system tables of MySQL Server
--

set @have_innodb= (select count(engine) from information_schema.engines where engine='INNODB' and support != 'NO');
set @is_mysql_encrypted = (select ENCRYPTION from information_schema.INNODB_TABLESPACES where NAME='mysql');

-- Tables below are NOT treated as DD tables by MySQL server yet.

SET FOREIGN_KEY_CHECKS= 1;

# Added sql_mode elements and making it as SET, instead of ENUM

set default_storage_engine=InnoDB;

SET @cmd = "CREATE TABLE IF NOT EXISTS db
(
Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL,
User char(32) binary DEFAULT '' NOT NULL,
Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
PRIMARY KEY Host (Host,Db,User), KEY User (User)
)
engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Database privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


-- Remember for later if db table already existed
set @had_db_table= @@warning_count != 0;

SET @cmd = "CREATE TABLE IF NOT EXISTS user
(
Host char(60) binary DEFAULT '' NOT NULL,
User char(32) binary DEFAULT '' NOT NULL,
Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL,
ssl_cipher BLOB NOT NULL,
x509_issuer BLOB NOT NULL,
x509_subject BLOB NOT NULL,
max_questions int(11) unsigned DEFAULT 0  NOT NULL,
max_updates int(11) unsigned DEFAULT 0  NOT NULL,
max_connections int(11) unsigned DEFAULT 0  NOT NULL,
max_user_connections int(11) unsigned DEFAULT 0  NOT NULL,
plugin char(64) DEFAULT 'caching_sha2_password' NOT NULL,
authentication_string TEXT,
password_expired ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
password_last_changed timestamp NULL DEFAULT NULL,
password_lifetime smallint unsigned NULL DEFAULT NULL,
account_locked ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_role_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_role_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Password_reuse_history smallint unsigned NULL DEFAULT NULL,
Password_reuse_time smallint unsigned NULL DEFAULT NULL,
Password_require_current enum('N', 'Y') COLLATE utf8_general_ci DEFAULT NULL,
User_attributes JSON DEFAULT NULL,
PRIMARY KEY Host (Host,User)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Users and global privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd = "CREATE TABLE IF NOT EXISTS default_roles
(
HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
USER CHAR(32) BINARY DEFAULT '' NOT NULL,
DEFAULT_ROLE_HOST CHAR(60) BINARY DEFAULT '%' NOT NULL,
DEFAULT_ROLE_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
PRIMARY KEY (HOST, USER, DEFAULT_ROLE_HOST, DEFAULT_ROLE_USER)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Default roles' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd = "CREATE TABLE IF NOT EXISTS role_edges
(
FROM_HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
FROM_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
TO_HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
TO_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
WITH_ADMIN_OPTION ENUM('N', 'Y') COLLATE UTF8_GENERAL_CI DEFAULT 'N' NOT NULL,
PRIMARY KEY (FROM_HOST,FROM_USER,TO_HOST,TO_USER)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Role hierarchy and role grants' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd = "CREATE TABLE IF NOT EXISTS global_grants
(
USER CHAR(32) BINARY DEFAULT '' NOT NULL,
HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
PRIV CHAR(32) COLLATE UTF8_GENERAL_CI DEFAULT '' NOT NULL,
WITH_GRANT_OPTION ENUM('N','Y') COLLATE UTF8_GENERAL_CI DEFAULT 'N' NOT NULL,
PRIMARY KEY (USER,HOST,PRIV)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Extended global grants' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


-- Remember for later if user table already existed
set @had_user_table= @@warning_count != 0;

SET @cmd = "CREATE TABLE IF NOT EXISTS password_history
(
  Host CHAR(60) BINARY DEFAULT '' NOT NULL,
  User CHAR(32) BINARY DEFAULT '' NOT NULL,
  Password_timestamp TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  Password TEXT,
  PRIMARY KEY(Host, User, Password_timestamp DESC)
 ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin
 comment='Password history for user accounts' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS func (  name char(64) binary DEFAULT '' NOT NULL, ret tinyint(1) DEFAULT '0' NOT NULL, dl char(128) DEFAULT '' NOT NULL, type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL, PRIMARY KEY (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='User defined functions' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS plugin ( name varchar(64) DEFAULT '' NOT NULL, dl varchar(128) DEFAULT '' NOT NULL, PRIMARY KEY (name) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_general_ci comment='MySQL plugins' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS servers ( Server_name char(64) NOT NULL DEFAULT '', Host char(64) NOT NULL DEFAULT '', Db char(64) NOT NULL DEFAULT '', Username char(64) NOT NULL DEFAULT '', Password char(64) NOT NULL DEFAULT '', Port INT(4) NOT NULL DEFAULT '0', Socket char(64) NOT NULL DEFAULT '', Wrapper char(64) NOT NULL DEFAULT '', Owner char(64) NOT NULL DEFAULT '', PRIMARY KEY (Server_name)) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='MySQL Foreign Servers table' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS tables_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Table privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd = "CREATE TABLE IF NOT EXISTS columns_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Column privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS help_topic ( help_topic_id int unsigned not null, name char(64) not null, help_category_id smallint unsigned not null, description text not null, example text not null, url text not null, primary key (help_topic_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help topics' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS help_category ( help_category_id smallint unsigned not null, name  char(64) not null, parent_category_id smallint unsigned null, url text not null, primary key (help_category_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help categories' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS help_relation ( help_topic_id int unsigned not null, help_keyword_id  int unsigned not null, primary key (help_keyword_id, help_topic_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='keyword-topic relation' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS help_keyword (   help_keyword_id  int unsigned not null, name char(64) not null, primary key (help_keyword_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help keywords' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS time_zone_name (   Name char(64) NOT NULL, Time_zone_id int unsigned NOT NULL, PRIMARY KEY Name (Name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone names' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS time_zone (   Time_zone_id int unsigned NOT NULL auto_increment, Use_leap_seconds enum('Y','N') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY TzId (Time_zone_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zones' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS time_zone_transition (   Time_zone_id int unsigned NOT NULL, Transition_time bigint signed NOT NULL, Transition_type_id int unsigned NOT NULL, PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transitions' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS time_zone_transition_type (   Time_zone_id int unsigned NOT NULL, Transition_type_id int unsigned NOT NULL, Offset int signed DEFAULT 0 NOT NULL, Is_DST tinyint unsigned DEFAULT 0 NOT NULL, Abbreviation char(8) DEFAULT '' NOT NULL, PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transition types' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS time_zone_leap_second (   Transition_time bigint signed NOT NULL, Correction int signed NOT NULL, PRIMARY KEY TranTime (Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Leap seconds information for time zones' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;



SET @cmd = "CREATE TABLE IF NOT EXISTS procs_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Routine_name char(64) COLLATE utf8_general_ci DEFAULT '' NOT NULL, Routine_type enum('FUNCTION','PROCEDURE') NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY (Host,Db,User,Routine_name,Routine_type), KEY Grantor (Grantor) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Procedure privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


-- bug#92988: Temporarily turn off sql_require_primary_key for the
-- next 2 tables as this is not necessary as they do not replicate.
SET @old_sql_require_primary_key = @@session.sql_require_primary_key;
SET @@session.sql_require_primary_key = 0;

-- Create general_log
CREATE TABLE IF NOT EXISTS general_log (event_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL, server_id INTEGER UNSIGNED NOT NULL, command_type VARCHAR(64) NOT NULL, argument MEDIUMBLOB NOT NULL) engine=CSV CHARACTER SET utf8 comment="General log";

-- Create slow_log
CREATE TABLE IF NOT EXISTS slow_log (start_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, query_time TIME(6) NOT NULL, lock_time TIME(6) NOT NULL, rows_sent INTEGER NOT NULL, rows_examined INTEGER NOT NULL, db VARCHAR(512) NOT NULL, last_insert_id INTEGER NOT NULL, insert_id INTEGER NOT NULL, server_id INTEGER UNSIGNED NOT NULL, sql_text MEDIUMBLOB NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL) engine=CSV CHARACTER SET utf8 comment="Slow log";

SET @@session.sql_require_primary_key = @old_sql_require_primary_key;

SET @cmd = "CREATE TABLE IF NOT EXISTS component ( component_id int unsigned NOT NULL AUTO_INCREMENT, component_group_id int unsigned NOT NULL, component_urn text NOT NULL, PRIMARY KEY (component_id)) engine=INNODB DEFAULT CHARSET=utf8 COMMENT 'Components' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd="CREATE TABLE IF NOT EXISTS slave_relay_log_info (
  Number_of_lines INTEGER UNSIGNED NOT NULL COMMENT 'Number of lines in the file or rows in the table. Used to version table definitions.', 
  Relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the current relay log file.', 
  Relay_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The relay log position of the last executed event.', 
  Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the master binary log file from which the events in the relay log file were read.', 
  Master_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The master log position of the last executed event.', 
  Sql_delay INTEGER NOT NULL COMMENT 'The number of seconds that the slave must lag behind the master.', 
  Number_of_workers INTEGER UNSIGNED NOT NULL,
  Id INTEGER UNSIGNED NOT NULL COMMENT 'Internal Id that uniquely identifies this record.',
  Channel_name CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL COMMENT 'The channel on which the slave is connected to a source. Used in Multisource Replication',
  PRIMARY KEY(Channel_name)) DEFAULT CHARSET=utf8 STATS_PERSISTENT=0 COMMENT 'Relay Log Information'";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB TABLESPACE=mysql ENCRYPTION=\'', @is_mysql_encrypted,'\''), CONCAT(@cmd, ' ENGINE= MYISAM'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE TABLE IF NOT EXISTS slave_master_info (
  Number_of_lines INTEGER UNSIGNED NOT NULL COMMENT 'Number of lines in the file.', 
  Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the master binary log currently being read from the master.', 
  Master_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The master log position of the last read event.', 
  Host CHAR(64) CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The host name of the master.', 
  User_name TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The user name used to connect to the master.', 
  User_password TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The password used to connect to the master.', 
  Port INTEGER UNSIGNED NOT NULL COMMENT 'The network port used to connect to the master.', 
  Connect_retry INTEGER UNSIGNED NOT NULL COMMENT 'The period (in seconds) that the slave will wait before trying to reconnect to the master.', 
  Enabled_ssl BOOLEAN NOT NULL COMMENT 'Indicates whether the server supports SSL connections.', 
  Ssl_ca TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The file used for the Certificate Authority (CA) certificate.', 
  Ssl_capath TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The path to the Certificate Authority (CA) certificates.', 
  Ssl_cert TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the SSL certificate file.', 
  Ssl_cipher TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the cipher in use for the SSL connection.', 
  Ssl_key TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the SSL key file.', 
  Ssl_verify_server_cert BOOLEAN NOT NULL COMMENT 'Whether to verify the server certificate.', 
  Heartbeat FLOAT NOT NULL COMMENT '', 
  Bind TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'Displays which interface is employed when connecting to the MySQL server', 
  Ignored_server_ids TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The number of server IDs to be ignored, followed by the actual server IDs', 
  Uuid TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The master server uuid.', 
  Retry_count BIGINT UNSIGNED NOT NULL COMMENT 'Number of reconnect attempts, to the master, before giving up.', 
  Ssl_crl TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The file used for the Certificate Revocation List (CRL)', 
  Ssl_crlpath TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The path used for Certificate Revocation List (CRL) files', 
  Enabled_auto_position BOOLEAN NOT NULL COMMENT 'Indicates whether GTIDs will be used to retrieve events from the master.',
  Channel_name CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL COMMENT 'The channel on which the slave is connected to a source. Used in Multisource Replication',
  Tls_version TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'Tls version',
  Public_key_path TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The file containing public key of master server.',
  Get_public_key BOOLEAN NOT NULL COMMENT 'Preference to get public key from master.',
  Network_namespace TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'Network namespace used for communication with the master server.',
  PRIMARY KEY(Channel_name)) DEFAULT CHARSET=utf8 STATS_PERSISTENT=0 COMMENT 'Master Information'";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB TABLESPACE=mysql ENCRYPTION=\'', @is_mysql_encrypted,'\''), CONCAT(@cmd, ' ENGINE= MYISAM'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE TABLE IF NOT EXISTS slave_worker_info (
  Id INTEGER UNSIGNED NOT NULL, 
  Relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, 
  Relay_log_pos BIGINT UNSIGNED NOT NULL, 
  Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, 
  Master_log_pos BIGINT UNSIGNED NOT NULL, 
  Checkpoint_relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, 
  Checkpoint_relay_log_pos BIGINT UNSIGNED NOT NULL, 
  Checkpoint_master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, 
  Checkpoint_master_log_pos BIGINT UNSIGNED NOT NULL, 
  Checkpoint_seqno INT UNSIGNED NOT NULL, 
  Checkpoint_group_size INTEGER UNSIGNED NOT NULL, 
  Checkpoint_group_bitmap BLOB NOT NULL, 
  Channel_name CHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL COMMENT 'The channel on which the slave is connected to a source. Used in Multisource Replication',
  PRIMARY KEY(Channel_name, Id)) DEFAULT CHARSET=utf8 STATS_PERSISTENT=0 COMMENT 'Worker Information'";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB TABLESPACE=mysql ENCRYPTION=\'', @is_mysql_encrypted,'\''), CONCAT(@cmd, ' ENGINE= MYISAM'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE TABLE IF NOT EXISTS gtid_executed (
    source_uuid CHAR(36) NOT NULL COMMENT 'uuid of the source where the transaction was originally executed.',
    interval_start BIGINT NOT NULL COMMENT 'First number of interval.',
    interval_end BIGINT NOT NULL COMMENT 'Last number of interval.',
    PRIMARY KEY(source_uuid, interval_start))";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB TABLESPACE=mysql ENCRYPTION=\'', @is_mysql_encrypted,'\''), CONCAT(@cmd, ' ENGINE= MYISAM'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- Optimizer Cost Model configuration
-- (Note: Column definition for default_value needs to be updated when a
--        default value is changed).
--

-- Server cost constants

SET @cmd = "CREATE TABLE IF NOT EXISTS server_cost (
  cost_name   VARCHAR(64) NOT NULL,
  cost_value  FLOAT DEFAULT NULL,
  last_update TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  comment     VARCHAR(1024) DEFAULT NULL,
  default_value FLOAT GENERATED ALWAYS AS
    (CASE cost_name
       WHEN 'disk_temptable_create_cost' THEN 20.0
       WHEN 'disk_temptable_row_cost' THEN 0.5
       WHEN 'key_compare_cost' THEN 0.05
       WHEN 'memory_temptable_create_cost' THEN 1.0
       WHEN 'memory_temptable_row_cost' THEN 0.1
       WHEN 'row_evaluate_cost' THEN 0.1
       ELSE NULL
     END) VIRTUAL,
  PRIMARY KEY (cost_name)
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci STATS_PERSISTENT=0 TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


INSERT IGNORE INTO server_cost(cost_name) VALUES
  ("row_evaluate_cost"), ("key_compare_cost"),
  ("memory_temptable_create_cost"), ("memory_temptable_row_cost"),
  ("disk_temptable_create_cost"), ("disk_temptable_row_cost");

-- Engine cost constants

SET @cmd = "CREATE TABLE IF NOT EXISTS engine_cost (
  engine_name VARCHAR(64) NOT NULL,
  device_type INTEGER NOT NULL,
  cost_name   VARCHAR(64) NOT NULL,
  cost_value  FLOAT DEFAULT NULL,
  last_update TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  comment     VARCHAR(1024) DEFAULT NULL,
  default_value FLOAT GENERATED ALWAYS AS
    (CASE cost_name
       WHEN 'io_block_read_cost' THEN 1.0
       WHEN 'memory_block_read_cost' THEN 0.25
       ELSE NULL
     END) VIRTUAL,
  PRIMARY KEY (cost_name, engine_name, device_type)
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci STATS_PERSISTENT=0 TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


INSERT IGNORE INTO engine_cost(engine_name, device_type, cost_name) VALUES
  ("default", 0, "memory_block_read_cost"),
  ("default", 0, "io_block_read_cost");


SET @cmd = "CREATE TABLE IF NOT EXISTS proxies_priv (Host char(60) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Proxied_host char(60) binary DEFAULT '' NOT NULL, Proxied_user char(32) binary DEFAULT '' NOT NULL, With_grant BOOL DEFAULT 0 NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY Host (Host,User,Proxied_host,Proxied_user), KEY Grantor (Grantor) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='User proxy privileges' TABLESPACE=mysql";
SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


-- Remember for later if proxies_priv table already existed
set @had_proxies_priv_table= @@warning_count != 0;


--
-- Only create the ndb_binlog_index table if the server is built with ndb.
-- Create this table last among the tables in the mysql schema to make it
-- easier to keep tests agnostic wrt. the existence of this table.
--
SET @have_ndb= (select count(engine) from information_schema.engines where engine='ndbcluster');
SET @cmd="CREATE TABLE IF NOT EXISTS ndb_binlog_index (
  Position BIGINT UNSIGNED NOT NULL,
  File VARCHAR(255) NOT NULL,
  epoch BIGINT UNSIGNED NOT NULL,
  inserts INT UNSIGNED NOT NULL,
  updates INT UNSIGNED NOT NULL,
  deletes INT UNSIGNED NOT NULL,
  schemaops INT UNSIGNED NOT NULL,
  orig_server_id INT UNSIGNED NOT NULL,
  orig_epoch BIGINT UNSIGNED NOT NULL,
  gci INT UNSIGNED NOT NULL,
  next_position BIGINT UNSIGNED NOT NULL,
  next_file VARCHAR(255) NOT NULL,
  PRIMARY KEY(epoch, orig_server_id, orig_epoch)
  ) ENGINE=INNODB CHARACTER SET latin1 STATS_PERSISTENT=0 TABLESPACE=mysql";

SET @str = CONCAT(@cmd, " ENCRYPTION='", @is_mysql_encrypted, "'");
SET @str = IF(@have_ndb = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Temporarily turn off sql_require_primary_key for the ndbinfo tables
SET @old_sql_require_primary_key = @@session.sql_require_primary_key;
SET @@session.sql_require_primary_key = 0;

# Generated by ndbinfo_sql # DO NOT EDIT! # Begin
# TABLE definitions from src/kernel/vm/NdbinfoTables.cpp
# VIEW definitions from tools/ndbinfo_sql.cpp
#
# SQL commands for creating the tables in MySQL Server which
# are used by the NDBINFO storage engine to access system
# information and statistics from MySQL Cluster
#

# Use latin1 when creating ndbinfo objects
SET NAMES 'latin1' COLLATE 'latin1_swedish_ci';

# Only create objects if NDBINFO is supported
SELECT @have_ndbinfo:= COUNT(*) FROM information_schema.engines WHERE engine='NDBINFO' AND support IN ('YES', 'DEFAULT');

# Only create objects if version >= 7.1
SET @str=IF(@have_ndbinfo,'SELECT @have_ndbinfo:= (@@ndbinfo_version >= (7 << 16) | (1 << 8)) || @ndbinfo_skip_version_check','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE DATABASE IF NOT EXISTS `ndbinfo`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Set NDBINFO in offline mode during (re)create of tables
# and views to avoid errors caused by no such table or
# different table definition in NDB
SET @str=IF(@have_ndbinfo,'SET @@global.ndbinfo_offline=TRUE','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Drop obsolete lookups in ndbinfo
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Drop any old views in ndbinfo
SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`arbitrator_validity_detail`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`arbitrator_validity_summary`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cluster_locks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cluster_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cluster_transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`config_nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`config_values`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`counters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cpustat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cpustat_1sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cpustat_20sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cpustat_50ms`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`dict_obj_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`disk_write_speed_aggregate_node`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`error_messages`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`locks_per_fragment`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`memory_per_fragment`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`memoryusage`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`operations_per_fragment`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`processes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`server_locks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`server_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`server_transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`table_distribution_status`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`table_fragments`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`table_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`table_replicas`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`tc_time_track_stats`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`threadblocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`threads`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Drop any old lookup tables in ndbinfo
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$error_messages`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Recreate lookup tables in ndbinfo
# ndbinfo.ndb$acc_operations
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$acc_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$acc_operations` (`node_id` INT UNSIGNED COMMENT "node_id",`block_instance` INT UNSIGNED COMMENT "Block instance",`tableid` INT UNSIGNED COMMENT "Table id",`fragmentid` INT UNSIGNED COMMENT "Fragment id",`rowid` BIGINT UNSIGNED COMMENT "Row id in fragment",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`acc_op_id` INT UNSIGNED COMMENT "Operation id",`op_flags` INT UNSIGNED COMMENT "Operation flags",`prev_serial_op_id` INT UNSIGNED COMMENT "Prev serial op id",`next_serial_op_id` INT UNSIGNED COMMENT "Next serial op id",`prev_parallel_op_id` INT UNSIGNED COMMENT "Prev parallel op id",`next_parallel_op_id` INT UNSIGNED COMMENT "Next parallel op id",`duration_millis` INT UNSIGNED COMMENT "Duration of wait/hold",`user_ptr` INT UNSIGNED COMMENT "Lock requestor context") COMMENT="ACC operation info" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$columns
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$columns`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$columns` (`table_id` INT UNSIGNED,`column_id` INT UNSIGNED,`column_name` VARCHAR(512),`column_type` INT UNSIGNED,`comment` VARCHAR(512)) COMMENT="metadata for columns available through ndbinfo " ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$config_nodes
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_nodes` (`reporting_node_id` INT UNSIGNED COMMENT "Reporting data node ID",`node_id` INT UNSIGNED COMMENT "Configured node ID",`node_type` INT UNSIGNED COMMENT "Configured node type",`node_hostname` VARCHAR(512) COMMENT "Configured hostname") COMMENT="All nodes of current cluster configuration" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$config_values
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_values`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_values` (`node_id` INT UNSIGNED,`config_param` INT UNSIGNED COMMENT "Parameter number",`config_value` VARCHAR(512) COMMENT "Parameter value") COMMENT="Configuration parameter values" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$counters
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$counters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$counters` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`counter_id` INT UNSIGNED,`val` BIGINT UNSIGNED COMMENT "monotonically increasing since process start") COMMENT="monotonic counters" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user` INT UNSIGNED COMMENT "Percentage time spent in user mode as reported by OS",`OS_system` INT UNSIGNED COMMENT "Percentage time spent in system mode as reported by OS",`OS_idle` INT UNSIGNED COMMENT "Percentage time spent in idle mode as reported by OS",`thread_exec` INT UNSIGNED COMMENT "Percentage time spent executing as calculated by thread",`thread_sleeping` INT UNSIGNED COMMENT "Percentage time spent sleeping as calculated by thread",`thread_spinning` INT UNSIGNED COMMENT "Percentage time spent spinning as calculated by thread",`thread_send` INT UNSIGNED COMMENT "Percentage time spent sending as calculated by thread",`thread_buffer_full` INT UNSIGNED COMMENT "Percentage time spent in buffer full as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats for last second" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_1sec
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_1sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_1sec` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 1 second intervals" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_20sec
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_20sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_20sec` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 20 seconds intervals" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_50ms
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_50ms`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_50ms` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 50 milliseconds intervals" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dict_obj_info
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dict_obj_info` (`type` INT UNSIGNED COMMENT "Type of dict object",`id` INT UNSIGNED COMMENT "Object identity",`version` INT UNSIGNED COMMENT "Object version",`state` INT UNSIGNED COMMENT "Object state",`parent_obj_type` INT UNSIGNED COMMENT "Parent object type",`parent_obj_id` INT UNSIGNED COMMENT "Parent object id",`fq_name` VARCHAR(512) COMMENT "Fully qualified object name") COMMENT="Dictionary object info" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$disk_write_speed_aggregate
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_aggregate` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`backup_lcp_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP last second",`redo_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log last second",`backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 10 seconds",`redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 10 seconds",`std_dev_backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 10 seconds",`std_dev_redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 10 seconds",`backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 60 seconds",`redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 60 seconds",`std_dev_backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 60 seconds",`std_dev_redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 60 seconds",`slowdowns_due_to_io_lag` BIGINT UNSIGNED COMMENT "Number of seconds that we slowed down disk writes due to REDO log IO lagging",`slowdowns_due_to_high_cpu` BIGINT UNSIGNED COMMENT "Number of seconds we slowed down disk writes due to high CPU usage of LDM thread",`disk_write_speed_set_to_min` BIGINT UNSIGNED COMMENT "Number of seconds we set disk write speed to a minimum",`current_target_disk_write_speed` BIGINT UNSIGNED COMMENT "Current target of disk write speed in bytes per second") COMMENT="Actual speed of disk writes per LDM thread, aggregate data" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$disk_write_speed_base
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_base` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`millis_ago` BIGINT UNSIGNED COMMENT "Milliseconds ago since this period finished",`millis_passed` BIGINT UNSIGNED COMMENT "Milliseconds passed in the period reported",`backup_lcp_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written by backup and LCP in the period",`redo_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written to REDO log in the period",`target_disk_write_speed` BIGINT UNSIGNED COMMENT "Target disk write speed in bytes per second at the measurement point") COMMENT="Actual speed of disk writes per LDM thread, base data" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$diskpagebuffer
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$diskpagebuffer` (`node_id` INT UNSIGNED,`block_instance` INT UNSIGNED,`pages_written` BIGINT UNSIGNED COMMENT "Pages written to disk",`pages_written_lcp` BIGINT UNSIGNED COMMENT "Pages written by local checkpoint",`pages_read` BIGINT UNSIGNED COMMENT "Pages read from disk",`log_waits` BIGINT UNSIGNED COMMENT "Page writes waiting for log to be written to disk",`page_requests_direct_return` BIGINT UNSIGNED COMMENT "Page in buffer and no requests waiting for it",`page_requests_wait_queue` BIGINT UNSIGNED COMMENT "Page in buffer, but some requests are already waiting for it",`page_requests_wait_io` BIGINT UNSIGNED COMMENT "Page not in buffer, waiting to be read from disk") COMMENT="disk page buffer info" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$frag_locks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_locks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_locks` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`ex_req` BIGINT UNSIGNED COMMENT "Exclusive row lock request count",`ex_imm_ok` BIGINT UNSIGNED COMMENT "Exclusive row lock immediate grants",`ex_wait_ok` BIGINT UNSIGNED COMMENT "Exclusive row lock grants with wait",`ex_wait_fail` BIGINT UNSIGNED COMMENT "Exclusive row lock failed grants",`sh_req` BIGINT UNSIGNED COMMENT "Shared row lock request count",`sh_imm_ok` BIGINT UNSIGNED COMMENT "Shared row lock immediate grants",`sh_wait_ok` BIGINT UNSIGNED COMMENT "Shared row lock grants with wait",`sh_wait_fail` BIGINT UNSIGNED COMMENT "Shared row lock failed grants",`wait_ok_millis` BIGINT UNSIGNED COMMENT "Time spent waiting before successfully claiming a lock",`wait_fail_millis` BIGINT UNSIGNED COMMENT "Time spent waiting before failing to claim a lock") COMMENT="Per fragment lock information" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$frag_mem_use
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_mem_use`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_mem_use` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LDM instance number",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`rows` BIGINT UNSIGNED COMMENT "Number of rows in table",`fixed_elem_alloc_bytes` BIGINT UNSIGNED COMMENT "Number of bytes allocated for fixed-sized elements",`fixed_elem_free_bytes` BIGINT UNSIGNED COMMENT "Free bytes in fixed-size element pages",`fixed_elem_count` BIGINT UNSIGNED COMMENT "Number of fixed size elements in use",`fixed_elem_size_bytes` INT UNSIGNED COMMENT "Length of each fixed sized element in bytes",`var_elem_alloc_bytes` BIGINT UNSIGNED COMMENT "Number of bytes allocated for var-size elements",`var_elem_free_bytes` BIGINT UNSIGNED COMMENT "Free bytes in var-size element pages",`var_elem_count` BIGINT UNSIGNED COMMENT "Number of var size elements in use",`tuple_l2pmap_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in logical to physical page map for tuple store",`hash_index_l2pmap_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in logical to physical page map for the hash index",`hash_index_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in linear hash map") COMMENT="Per fragment space information" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$frag_operations
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_operations` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`tot_key_reads` BIGINT UNSIGNED COMMENT "Total number of key reads received",`tot_key_inserts` BIGINT UNSIGNED COMMENT "Total number of key inserts received",`tot_key_updates` BIGINT UNSIGNED COMMENT "Total number of key updates received",`tot_key_writes` BIGINT UNSIGNED COMMENT "Total number of key writes received",`tot_key_deletes` BIGINT UNSIGNED COMMENT "Total number of key deletes received",`tot_key_refs` BIGINT UNSIGNED COMMENT "Total number of key operations refused by LDM",`tot_key_attrinfo_bytes` BIGINT UNSIGNED COMMENT "Total attrinfo bytes received for key operations",`tot_key_keyinfo_bytes` BIGINT UNSIGNED COMMENT "Total keyinfo bytes received for key operations",`tot_key_prog_bytes` BIGINT UNSIGNED COMMENT "Total bytes of filter programs for key operations",`tot_key_inst_exec` BIGINT UNSIGNED COMMENT "Total number of interpreter instructions executed for key operations",`tot_key_bytes_returned` BIGINT UNSIGNED COMMENT "Total number of bytes returned to client for key operations",`tot_frag_scans` BIGINT UNSIGNED COMMENT "Total number of fragment scans received",`tot_scan_rows_examined` BIGINT UNSIGNED COMMENT "Total number of rows examined by scans",`tot_scan_rows_returned` BIGINT UNSIGNED COMMENT "Total number of rows returned to client by scan",`tot_scan_bytes_returned` BIGINT UNSIGNED COMMENT "Total number of bytes returned to client by scans",`tot_scan_prog_bytes` BIGINT UNSIGNED COMMENT "Total bytes of scan filter programs",`tot_scan_bound_bytes` BIGINT UNSIGNED COMMENT "Total bytes of scan bounds",`tot_scan_inst_exec` BIGINT UNSIGNED COMMENT "Total number of interpreter instructions executed for scans",`tot_qd_frag_scans` BIGINT UNSIGNED COMMENT "Total number of fragment scans queued before exec",`conc_frag_scans` INT UNSIGNED COMMENT "Number of frag scans currently running",`conc_qd_plain_frag_scans` INT UNSIGNED COMMENT "Number of tux frag scans currently queued",`conc_qd_tup_frag_scans` INT UNSIGNED COMMENT "Number of tup frag scans currently queued",`conc_qd_acc_frag_scans` INT UNSIGNED COMMENT "Number of acc frag scans currently queued",`tot_commits` BIGINT UNSIGNED COMMENT "Total number of committed row changes") COMMENT="Per fragment operational information" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$logbuffers
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$logbuffers` (`node_id` INT UNSIGNED,`log_type` INT UNSIGNED COMMENT "0 = REDO, 1 = DD-UNDO, 2 = BACKUP-DATA, 3 = BACKUP-LOG",`log_id` INT UNSIGNED,`log_part` INT UNSIGNED,`total` BIGINT UNSIGNED COMMENT "total allocated",`used` BIGINT UNSIGNED COMMENT "currently in use",`high` BIGINT UNSIGNED COMMENT "in use high water mark") COMMENT="logbuffer usage" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$logspaces
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$logspaces` (`node_id` INT UNSIGNED,`log_type` INT UNSIGNED COMMENT "0 = REDO, 1 = DD-UNDO",`log_id` INT UNSIGNED,`log_part` INT UNSIGNED,`total` BIGINT UNSIGNED COMMENT "total allocated",`used` BIGINT UNSIGNED COMMENT "currently in use",`high` BIGINT UNSIGNED COMMENT "in use high water mark") COMMENT="logspace usage" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$membership
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$membership` (`node_id` INT UNSIGNED COMMENT "node id",`group_id` INT UNSIGNED COMMENT "node group id",`left_node` INT UNSIGNED COMMENT "Left node in heart beat chain",`right_node` INT UNSIGNED COMMENT "Right node in heart beat chain",`president` INT UNSIGNED COMMENT "President nodeid",`successor` INT UNSIGNED COMMENT "President successor",`dynamic_id` INT UNSIGNED COMMENT "President, Configured_heartbeat order",`arbitrator` INT UNSIGNED COMMENT "Arbitrator nodeid",`arb_ticket` VARCHAR(512) COMMENT "Arbitrator ticket",`arb_state` INT UNSIGNED COMMENT "Arbitrator state",`arb_connected` INT UNSIGNED COMMENT "Arbitrator connected",`conn_rank1_arbs` VARCHAR(512) COMMENT "Connected rank 1 arbitrators",`conn_rank2_arbs` VARCHAR(512) COMMENT "Connected rank 2 arbitrators") COMMENT="membership" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$nodes
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$nodes` (`node_id` INT UNSIGNED,`uptime` BIGINT UNSIGNED COMMENT "time in seconds that node has been running",`status` INT UNSIGNED COMMENT "starting/started/stopped etc.",`start_phase` INT UNSIGNED COMMENT "start phase if node is starting",`config_generation` INT UNSIGNED COMMENT "configuration generation number") COMMENT="node status" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$operations
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$operations` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`objid` INT UNSIGNED COMMENT "Object id of operation object",`tcref` INT UNSIGNED COMMENT "TC reference",`apiref` INT UNSIGNED COMMENT "API reference",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`tableid` INT UNSIGNED COMMENT "Table id",`fragmentid` INT UNSIGNED COMMENT "Fragment id",`op` INT UNSIGNED COMMENT "Operation type",`state` INT UNSIGNED COMMENT "Operation state",`flags` INT UNSIGNED COMMENT "Operation flags") COMMENT="operations" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$pools
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$pools`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$pools` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`pool_name` VARCHAR(512),`used` BIGINT UNSIGNED COMMENT "currently in use",`total` BIGINT UNSIGNED COMMENT "total allocated",`high` BIGINT UNSIGNED COMMENT "in use high water mark",`entry_size` BIGINT UNSIGNED COMMENT "size in bytes of each object",`config_param1` INT UNSIGNED COMMENT "config param 1 affecting pool",`config_param2` INT UNSIGNED COMMENT "config param 2 affecting pool",`config_param3` INT UNSIGNED COMMENT "config param 3 affecting pool",`config_param4` INT UNSIGNED COMMENT "config param 4 affecting pool",`resource_id` INT UNSIGNED,`type_id` INT UNSIGNED COMMENT "Record type id within resource") COMMENT="pool usage" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$processes
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$processes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$processes` (`reporting_node_id` INT UNSIGNED COMMENT "Reporting data node ID",`node_id` INT UNSIGNED COMMENT "Connected node ID",`node_type` INT UNSIGNED COMMENT "Type of node",`node_version` VARCHAR(512) COMMENT "Node MySQL Cluster version string",`process_id` INT UNSIGNED COMMENT "PID of node process on host",`angel_process_id` INT UNSIGNED COMMENT "PID of node\'s angel process",`process_name` VARCHAR(512) COMMENT "Node\'s executable process name",`service_URI` VARCHAR(512) COMMENT "URI for service provided by node") COMMENT="Process ID and Name information for connected nodes" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$resources
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$resources` (`node_id` INT UNSIGNED,`resource_id` INT UNSIGNED,`reserved` INT UNSIGNED COMMENT "reserved for this resource",`used` INT UNSIGNED COMMENT "currently in use",`max` INT UNSIGNED COMMENT "max available",`high` INT UNSIGNED COMMENT "in use high water mark",`spare` INT UNSIGNED COMMENT "spare pages for restart") COMMENT="resources usage (a.k.a superpool)" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$restart_info
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$restart_info` (`node_id` INT UNSIGNED COMMENT "node id",`node_restart_status` VARCHAR(512) COMMENT "Current state of node recovery",`node_restart_status_int` INT UNSIGNED COMMENT "Current state of node recovery as number",`secs_to_complete_node_failure` INT UNSIGNED COMMENT "Seconds to complete node failure handling",`secs_to_allocate_node_id` INT UNSIGNED COMMENT "Seconds from node failure completion to allocation of node id",`secs_to_include_in_heartbeat_protocol` INT UNSIGNED COMMENT "Seconds from allocation of node id to inclusion in HB protocol",`secs_until_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds from included in HB protocol until we wait for ndbcntr master",`secs_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds we waited for being accepted by NDBCNTR master to start",`secs_to_get_start_permitted` INT UNSIGNED COMMENT "Seconds from permit by master until all nodes accepted our start",`secs_to_wait_for_lcp_for_copy_meta_data` INT UNSIGNED COMMENT "Seconds waiting for LCP completion before copying meta data",`secs_to_copy_meta_data` INT UNSIGNED COMMENT "Seconds to copy meta data to starting node from master",`secs_to_include_node` INT UNSIGNED COMMENT "Seconds to wait for GCP and inclusion of all nodes into protocols",`secs_starting_node_to_request_local_recovery` INT UNSIGNED COMMENT "Seconds for starting node to request local recovery",`secs_for_local_recovery` INT UNSIGNED COMMENT "Seconds for local recovery in starting node",`secs_restore_fragments` INT UNSIGNED COMMENT "Seconds to restore fragments from LCP files",`secs_undo_disk_data` INT UNSIGNED COMMENT "Seconds to execute UNDO log on disk data part of records",`secs_exec_redo_log` INT UNSIGNED COMMENT "Seconds to execute REDO log on all restored fragments",`secs_index_rebuild` INT UNSIGNED COMMENT "Seconds to rebuild indexes on restored fragments",`secs_to_synchronize_starting_node` INT UNSIGNED COMMENT "Seconds to synchronize starting node from live nodes",`secs_wait_lcp_for_restart` INT UNSIGNED COMMENT "Seconds to wait for LCP start and completion before restart is completed",`secs_wait_subscription_handover` INT UNSIGNED COMMENT "Seconds waiting for handover of replication subscriptions",`total_restart_secs` INT UNSIGNED COMMENT "Total number of seconds from node failure until node is started again") COMMENT="Times of restart phases in seconds and current state" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$stored_tables
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$stored_tables`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$stored_tables` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`logged_table` INT UNSIGNED COMMENT "Is table logged",`row_contains_gci` INT UNSIGNED COMMENT "Does table rows contains GCI",`row_contains_checksum` INT UNSIGNED COMMENT "Does table rows contain checksum",`temporary_table` INT UNSIGNED COMMENT "Is table temporary",`force_var_part` INT UNSIGNED COMMENT "Force var part active",`read_backup` INT UNSIGNED COMMENT "Is backup replicas read",`fully_replicated` INT UNSIGNED COMMENT "Is table fully replicated",`extra_row_gci` INT UNSIGNED COMMENT "extra_row_gci",`extra_row_author` INT UNSIGNED COMMENT "extra_row_author",`storage_type` INT UNSIGNED COMMENT "Storage type of table",`hashmap_id` INT UNSIGNED COMMENT "Hashmap id",`hashmap_version` INT UNSIGNED COMMENT "Hashmap version",`table_version` INT UNSIGNED COMMENT "Table version",`fragment_type` INT UNSIGNED COMMENT "Type of fragmentation",`partition_balance` INT UNSIGNED COMMENT "Partition balance",`create_gci` INT UNSIGNED COMMENT "GCI in which table was created",`backup_locked` INT UNSIGNED COMMENT "Locked for backup",`single_user_mode` INT UNSIGNED COMMENT "Is single user mode active") COMMENT="Information about stored tables" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_distribution_status
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_distribution_status`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_distribution_status` (`node_id` INT UNSIGNED COMMENT "Node id",`table_id` INT UNSIGNED COMMENT "Table id",`tab_copy_status` INT UNSIGNED COMMENT "Copy status of the table",`tab_update_status` INT UNSIGNED COMMENT "Update status of the table",`tab_lcp_status` INT UNSIGNED COMMENT "LCP status of the table",`tab_status` INT UNSIGNED COMMENT "Create status of the table",`tab_storage` INT UNSIGNED COMMENT "Storage type of table",`tab_type` INT UNSIGNED COMMENT "Type of table",`tab_partitions` INT UNSIGNED COMMENT "Number of partitions in table",`tab_fragments` INT UNSIGNED COMMENT "Number of fragments in table",`current_scan_count` INT UNSIGNED COMMENT "Current number of active scans",`scan_count_wait` INT UNSIGNED COMMENT "Number of scans waiting for",`is_reorg_ongoing` INT UNSIGNED COMMENT "Is a table reorg ongoing on table") COMMENT="Table status in distribution handler" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_distribution_status_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_distribution_status_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_distribution_status_all` (`node_id` INT UNSIGNED COMMENT "Node id",`table_id` INT UNSIGNED COMMENT "Table id",`tab_copy_status` INT UNSIGNED COMMENT "Copy status of the table",`tab_update_status` INT UNSIGNED COMMENT "Update status of the table",`tab_lcp_status` INT UNSIGNED COMMENT "LCP status of the table",`tab_status` INT UNSIGNED COMMENT "Create status of the table",`tab_storage` INT UNSIGNED COMMENT "Storage type of table",`tab_type` INT UNSIGNED COMMENT "Type of table",`tab_partitions` INT UNSIGNED COMMENT "Number of partitions in table",`tab_fragments` INT UNSIGNED COMMENT "Number of fragments in table",`current_scan_count` INT UNSIGNED COMMENT "Current number of active scans",`scan_count_wait` INT UNSIGNED COMMENT "Number of scans waiting for",`is_reorg_ongoing` INT UNSIGNED COMMENT "Is a table reorg ongoing on table") COMMENT="Table status in distribution handler" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_fragments
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_fragments`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_fragments` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`partition_id` INT UNSIGNED COMMENT "Partition id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`partition_order` INT UNSIGNED COMMENT "Order of fragment in partition",`log_part_id` INT UNSIGNED COMMENT "Log part id of fragment",`no_of_replicas` INT UNSIGNED COMMENT "Number of replicas",`current_primary` INT UNSIGNED COMMENT "Current primary node id",`preferred_primary` INT UNSIGNED COMMENT "Preferred primary node id",`current_first_backup` INT UNSIGNED COMMENT "Current first backup node id",`current_second_backup` INT UNSIGNED COMMENT "Current second backup node id",`current_third_backup` INT UNSIGNED COMMENT "Current third backup node id",`num_alive_replicas` INT UNSIGNED COMMENT "Current number of alive replicas",`num_dead_replicas` INT UNSIGNED COMMENT "Current number of dead replicas",`num_lcp_replicas` INT UNSIGNED COMMENT "Number of replicas remaining to be LCP:ed") COMMENT="Partitions of the tables" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_fragments_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_fragments_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_fragments_all` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`partition_id` INT UNSIGNED COMMENT "Partition id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`partition_order` INT UNSIGNED COMMENT "Order of fragment in partition",`log_part_id` INT UNSIGNED COMMENT "Log part id of fragment",`no_of_replicas` INT UNSIGNED COMMENT "Number of replicas",`current_primary` INT UNSIGNED COMMENT "Current primary node id",`preferred_primary` INT UNSIGNED COMMENT "Preferred primary node id",`current_first_backup` INT UNSIGNED COMMENT "Current first backup node id",`current_second_backup` INT UNSIGNED COMMENT "Current second backup node id",`current_third_backup` INT UNSIGNED COMMENT "Current third backup node id",`num_alive_replicas` INT UNSIGNED COMMENT "Current number of alive replicas",`num_dead_replicas` INT UNSIGNED COMMENT "Current number of dead replicas",`num_lcp_replicas` INT UNSIGNED COMMENT "Number of replicas remaining to be LCP:ed") COMMENT="Partitions of the tables" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_replicas
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_replicas`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_replicas` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`initial_gci` INT UNSIGNED COMMENT "Initial GCI for table",`replica_node_id` INT UNSIGNED COMMENT "Node id where replica is stored",`is_lcp_ongoing` INT UNSIGNED COMMENT "Is LCP ongoing on this fragment",`num_crashed_replicas` INT UNSIGNED COMMENT "Number of crashed replica instances",`last_max_gci_started` INT UNSIGNED COMMENT "Last LCP Max GCI started",`last_max_gci_completed` INT UNSIGNED COMMENT "Last LCP Max GCI completed",`last_lcp_id` INT UNSIGNED COMMENT "Last LCP id",`prev_lcp_id` INT UNSIGNED COMMENT "Previous LCP id",`prev_max_gci_started` INT UNSIGNED COMMENT "Previous LCP Max GCI started",`prev_max_gci_completed` INT UNSIGNED COMMENT "Previous LCP Max GCI completed",`last_create_gci` INT UNSIGNED COMMENT "Last Create GCI of last crashed replica instance",`last_replica_gci` INT UNSIGNED COMMENT "Last GCI of last crashed replica instance",`is_replica_alive` INT UNSIGNED COMMENT "Is replica alive or not") COMMENT="Fragment replicas of the tables" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_replicas_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_replicas_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_replicas_all` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`initial_gci` INT UNSIGNED COMMENT "Initial GCI for table",`replica_node_id` INT UNSIGNED COMMENT "Node id where replica is stored",`is_lcp_ongoing` INT UNSIGNED COMMENT "Is LCP ongoing on this fragment",`num_crashed_replicas` INT UNSIGNED COMMENT "Number of crashed replica instances",`last_max_gci_started` INT UNSIGNED COMMENT "Last LCP Max GCI started",`last_max_gci_completed` INT UNSIGNED COMMENT "Last LCP Max GCI completed",`last_lcp_id` INT UNSIGNED COMMENT "Last LCP id",`prev_lcp_id` INT UNSIGNED COMMENT "Previous LCP id",`prev_max_gci_started` INT UNSIGNED COMMENT "Previous LCP Max GCI started",`prev_max_gci_completed` INT UNSIGNED COMMENT "Previous LCP Max GCI completed",`last_create_gci` INT UNSIGNED COMMENT "Last Create GCI of last crashed replica instance",`last_replica_gci` INT UNSIGNED COMMENT "Last GCI of last crashed replica instance",`is_replica_alive` INT UNSIGNED COMMENT "Is replica alive or not") COMMENT="Fragment replicas of the tables" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$tables
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$tables`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$tables` (`table_id` INT UNSIGNED,`table_name` VARCHAR(512),`comment` VARCHAR(512)) COMMENT="metadata for tables available through ndbinfo" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$tc_time_track_stats
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$tc_time_track_stats`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$tc_time_track_stats` (`node_id` INT UNSIGNED COMMENT "node id",`block_number` INT UNSIGNED COMMENT "Block number",`block_instance` INT UNSIGNED COMMENT "Block instance",`comm_node_id` INT UNSIGNED COMMENT "node_id of API or DB",`upper_bound` BIGINT UNSIGNED COMMENT "Upper bound in micros of interval",`scans` BIGINT UNSIGNED COMMENT "scan histogram interval",`scan_errors` BIGINT UNSIGNED COMMENT "scan error histogram interval",`scan_fragments` BIGINT UNSIGNED COMMENT "scan fragment histogram interval",`scan_fragment_errors` BIGINT UNSIGNED COMMENT "scan fragment error histogram interval",`transactions` BIGINT UNSIGNED COMMENT "transaction histogram interval",`transaction_errors` BIGINT UNSIGNED COMMENT "transaction error histogram interval",`read_key_ops` BIGINT UNSIGNED COMMENT "read key operation histogram interval",`write_key_ops` BIGINT UNSIGNED COMMENT "write key operation histogram interval",`index_key_ops` BIGINT UNSIGNED COMMENT "index key operation histogram interval",`key_op_errors` BIGINT UNSIGNED COMMENT "key operation error histogram interval") COMMENT="Time tracking of transaction, key operations and scan ops" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$test
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$test`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$test` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`counter` INT UNSIGNED,`counter2` BIGINT UNSIGNED) COMMENT="for testing" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threadblocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threadblocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threadblocks` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "thread number",`block_number` INT UNSIGNED COMMENT "block number",`block_instance` INT UNSIGNED COMMENT "block instance") COMMENT="which blocks are run in which threads" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threads
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threads`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threads` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`thread_name` VARCHAR(512) COMMENT "thread_name",`thread_description` VARCHAR(512) COMMENT "thread_description") COMMENT="Base table for threads" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threadstat
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threadstat` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "thread number",`thr_nm` VARCHAR(512) COMMENT "thread name",`c_loop` BIGINT UNSIGNED COMMENT "No of loops in main loop",`c_exec` BIGINT UNSIGNED COMMENT "No of signals executed",`c_wait` BIGINT UNSIGNED COMMENT "No of times waited for more input",`c_l_sent_prioa` BIGINT UNSIGNED COMMENT "No of prio A signals sent to own node",`c_l_sent_priob` BIGINT UNSIGNED COMMENT "No of prio B signals sent to own node",`c_r_sent_prioa` BIGINT UNSIGNED COMMENT "No of prio A signals sent to remote node",`c_r_sent_priob` BIGINT UNSIGNED COMMENT "No of prio B signals sent to remote node",`os_tid` BIGINT UNSIGNED COMMENT "OS thread id",`os_now` BIGINT UNSIGNED COMMENT "OS gettimeofday (millis)",`os_ru_utime` BIGINT UNSIGNED COMMENT "OS user CPU time (micros)",`os_ru_stime` BIGINT UNSIGNED COMMENT "OS system CPU time (micros)",`os_ru_minflt` BIGINT UNSIGNED COMMENT "OS page reclaims (soft page faults",`os_ru_majflt` BIGINT UNSIGNED COMMENT "OS page faults (hard page faults)",`os_ru_nvcsw` BIGINT UNSIGNED COMMENT "OS voluntary context switches",`os_ru_nivcsw` BIGINT UNSIGNED COMMENT "OS involuntary context switches") COMMENT="Statistics on execution threads" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$transactions
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$transactions` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "TC instance no",`objid` INT UNSIGNED COMMENT "Object id of transaction object",`apiref` INT UNSIGNED COMMENT "API reference",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`state` INT UNSIGNED COMMENT "Transaction state",`flags` INT UNSIGNED COMMENT "Transaction flags",`c_ops` INT UNSIGNED COMMENT "No of operations in transaction",`outstanding` INT UNSIGNED COMMENT "Currently outstanding request",`timer` INT UNSIGNED COMMENT "Timer (seconds)") COMMENT="transactions" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$transporters
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$transporters` (`node_id` INT UNSIGNED COMMENT "Node id reporting",`remote_node_id` INT UNSIGNED COMMENT "Node id at other end of link",`connection_status` INT UNSIGNED COMMENT "State of inter-node link",`remote_address` VARCHAR(512) COMMENT "Address of remote node",`bytes_sent` BIGINT UNSIGNED COMMENT "Bytes sent to remote node",`bytes_received` BIGINT UNSIGNED COMMENT "Bytes received from remote node",`connect_count` INT UNSIGNED COMMENT "Number of times connected",`overloaded` INT UNSIGNED COMMENT "Is link reporting overload",`overload_count` INT UNSIGNED COMMENT "Number of overload onsets since connect",`slowdown` INT UNSIGNED COMMENT "Is link requesting slowdown",`slowdown_count` INT UNSIGNED COMMENT "Number of slowdown onsets since connect") COMMENT="transporter status" ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Recreate handler local lookup tables in ndbinfo
# ndbinfo.ndb$blocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$blocks` (block_number INT UNSIGNED, block_name VARCHAR(512)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$config_params
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_params` (param_number INT UNSIGNED, param_name VARCHAR(512), param_description VARCHAR(512), param_type VARCHAR(512), param_default VARCHAR(512), param_min VARCHAR(512), param_max VARCHAR(512), param_mandatory INT UNSIGNED, param_status VARCHAR(512)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dblqh_tcconnect_state
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dblqh_tcconnect_state` (state_int_value INT UNSIGNED, state_name VARCHAR(256), state_friendly_name VARCHAR(256), state_description VARCHAR(256)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dbtc_apiconnect_state
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dbtc_apiconnect_state` (state_int_value INT UNSIGNED, state_name VARCHAR(256), state_friendly_name VARCHAR(256), state_description VARCHAR(256)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dict_obj_types
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dict_obj_types` (type_id INT UNSIGNED, type_name VARCHAR(512)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$error_messages
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$error_messages`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$error_messages` (error_code INT UNSIGNED, error_description VARCHAR(512), error_status VARCHAR(512), error_classification VARCHAR(512)) ENGINE=NDBINFO CHARACTER SET latin1','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Recreate views in ndbinfo
# ndbinfo.arbitrator_validity_detail
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`arbitrator_validity_detail` AS SELECT node_id, arbitrator, arb_ticket, CASE arb_connected  WHEN 1 THEN "Yes"  ELSE "No" END AS arb_connected, CASE arb_state  WHEN 0 THEN "ARBIT_NULL"  WHEN 1 THEN "ARBIT_INIT"  WHEN 2 THEN "ARBIT_FIND"  WHEN 3 THEN "ARBIT_PREP1"  WHEN 4 THEN "ARBIT_PREP2"  WHEN 5 THEN "ARBIT_START"  WHEN 6 THEN "ARBIT_RUN"  WHEN 7 THEN "ARBIT_CHOOSE"  WHEN 8 THEN "ARBIT_CRASH"  ELSE "UNKNOWN" END AS arb_state FROM `ndbinfo`.`ndb$membership` ORDER BY arbitrator, arb_connected DESC','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.arbitrator_validity_summary
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`arbitrator_validity_summary` AS SELECT arbitrator, arb_ticket, CASE arb_connected  WHEN 1 THEN "Yes"  ELSE "No" END AS arb_connected, count(*) as consensus_count FROM `ndbinfo`.`ndb$membership` GROUP BY arbitrator, arb_ticket, arb_connected','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.blocks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`blocks` AS SELECT block_number, block_name FROM `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cluster_locks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cluster_locks` AS SELECT `ndbinfo`.`ndb$acc_operations`.`node_id` AS `node_id`,`ndbinfo`.`ndb$acc_operations`.`block_instance` AS `block_instance`,`ndbinfo`.`ndb$acc_operations`.`tableid` AS `tableid`,`ndbinfo`.`ndb$acc_operations`.`fragmentid` AS `fragmentid`,`ndbinfo`.`ndb$acc_operations`.`rowid` AS `rowid`,`ndbinfo`.`ndb$acc_operations`.`transid0` + (`ndbinfo`.`ndb$acc_operations`.`transid1` << 32) AS `transid`,(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x10) when 0 then "S" else "X" end) AS `mode`,(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x80) when 0 then "W" else "H" end) AS `state`,(case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0x40) when 0 then "" else "*" end) as `detail`,case (`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0xf) when 0 then "READ" when 1 then "UPDATE" when 2 then "INSERT"when 3 then "DELETE" when 5 then "READ" when 6 then "REFRESH"when 7 then "UNLOCK" when 8 then "SCAN" ELSE"<unknown>" END as `op`,`ndbinfo`.`ndb$acc_operations`.`duration_millis` as `duration_millis`,`ndbinfo`.`ndb$acc_operations`.`acc_op_id` AS `lock_num`,if(`ndbinfo`.`ndb$acc_operations`.`op_flags` & 0xc0 = 0,`ndbinfo`.`ndb$acc_operations`.`prev_serial_op_id`, NULL) as `waiting_for` FROM `ndbinfo`.`ndb$acc_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cluster_operations
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cluster_operations` AS SELECT o.node_id, o.block_instance, o.transid0 + (o.transid1 << 32) as transid, case o.op  when 1 then "READ" when 2 then "READ-SH" when 3 then "READ-EX" when 4 then "INSERT" when 5 then "UPDATE" when 6 then "DELETE" when 7 then "WRITE" when 8 then "UNLOCK" when 9 then "REFRESH" when 257 then "SCAN" when 258 then "SCAN-SH" when 259 then "SCAN-EX" ELSE "<unknown>" END as operation_type,  s.state_friendly_name as state,  o.tableid,  o.fragmentid,  (o.apiref & 65535) as client_node_id,  (o.apiref >> 16) as client_block_ref,  (o.tcref & 65535) as tc_node_id,  ((o.tcref >> 16) & 511) as tc_block_no,  ((o.tcref >> (16 + 9)) & 127) as tc_block_instance FROM `ndbinfo`.`ndb$operations` o LEFT JOIN `ndbinfo`.`ndb$dblqh_tcconnect_state` s        ON s.state_int_value = o.state','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cluster_transactions
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cluster_transactions` AS SELECT t.node_id, t.block_instance, t.transid0 + (t.transid1 << 32) as transid, s.state_friendly_name as state,  t.c_ops as count_operations,  t.outstanding as outstanding_operations,  t.timer as inactive_seconds,  (t.apiref & 65535) as client_node_id,  (t.apiref >> 16) as client_block_ref FROM `ndbinfo`.`ndb$transactions` t LEFT JOIN `ndbinfo`.`ndb$dbtc_apiconnect_state` s        ON s.state_int_value = t.state','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.config_nodes
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`config_nodes` AS SELECT distinct node_id, CASE node_type  WHEN 0 THEN "NDB"  WHEN 1 THEN "API"  WHEN 2 THEN "MGM"  ELSE NULL  END AS node_type, node_hostname FROM `ndbinfo`.`ndb$config_nodes` ORDER BY node_id','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.config_params
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`config_params` AS SELECT param_number, param_name, param_description, param_type, param_default, param_min, param_max, param_mandatory, param_status FROM `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.config_values
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`config_values` AS SELECT node_id, config_param, config_value FROM `ndbinfo`.`ndb$config_values`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.counters
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`counters` AS SELECT node_id, b.block_name, block_instance, counter_id, CASE counter_id  WHEN 1 THEN "ATTRINFO"  WHEN 2 THEN "TRANSACTIONS"  WHEN 3 THEN "COMMITS"  WHEN 4 THEN "READS"  WHEN 5 THEN "SIMPLE_READS"  WHEN 6 THEN "WRITES"  WHEN 7 THEN "ABORTS"  WHEN 8 THEN "TABLE_SCANS"  WHEN 9 THEN "RANGE_SCANS"  WHEN 10 THEN "OPERATIONS"  WHEN 11 THEN "READS_RECEIVED"  WHEN 12 THEN "LOCAL_READS_SENT"  WHEN 13 THEN "REMOTE_READS_SENT"  WHEN 14 THEN "READS_NOT_FOUND"  WHEN 15 THEN "TABLE_SCANS_RECEIVED"  WHEN 16 THEN "LOCAL_TABLE_SCANS_SENT"  WHEN 17 THEN "RANGE_SCANS_RECEIVED"  WHEN 18 THEN "LOCAL_RANGE_SCANS_SENT"  WHEN 19 THEN "REMOTE_RANGE_SCANS_SENT"  WHEN 20 THEN "SCAN_BATCHES_RETURNED"  WHEN 21 THEN "SCAN_ROWS_RETURNED"  WHEN 22 THEN "PRUNED_RANGE_SCANS_RECEIVED"  WHEN 23 THEN "CONST_PRUNED_RANGE_SCANS_RECEIVED"  WHEN 24 THEN "LOCAL_READS"  WHEN 25 THEN "LOCAL_WRITES"  WHEN 26 THEN "LQHKEY_OVERLOAD"  WHEN 27 THEN "LQHKEY_OVERLOAD_TC"  WHEN 28 THEN "LQHKEY_OVERLOAD_READER"  WHEN 29 THEN "LQHKEY_OVERLOAD_NODE_PEER"  WHEN 30 THEN "LQHKEY_OVERLOAD_SUBSCRIBER"  WHEN 31 THEN "LQHSCAN_SLOWDOWNS"  ELSE "<unknown>"  END AS counter_name, val FROM `ndbinfo`.`ndb$counters` c LEFT JOIN `ndbinfo`.`ndb$blocks` b ON c.block_number = b.block_number','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cpustat
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cpustat` AS SELECT * FROM `ndbinfo`.`ndb$cpustat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cpustat_1sec
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cpustat_1sec` AS SELECT * FROM `ndbinfo`.`ndb$cpustat_1sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cpustat_20sec
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cpustat_20sec` AS SELECT * FROM `ndbinfo`.`ndb$cpustat_20sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cpustat_50ms
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cpustat_50ms` AS SELECT * FROM `ndbinfo`.`ndb$cpustat_50ms`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.dict_obj_info
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`dict_obj_info` AS  SELECT * FROM `ndbinfo`.`ndb$dict_obj_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.dict_obj_types
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`dict_obj_types` AS SELECT type_id, type_name FROM `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_aggregate
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_aggregate` AS SELECT * FROM `ndbinfo`.`ndb$disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_aggregate_node
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_aggregate_node` AS SELECT node_id, SUM(backup_lcp_speed_last_sec) AS backup_lcp_speed_last_sec, SUM(redo_speed_last_sec) AS redo_speed_last_sec, SUM(backup_lcp_speed_last_10sec) AS backup_lcp_speed_last_10sec, SUM(redo_speed_last_10sec) AS redo_speed_last_10sec, SUM(backup_lcp_speed_last_60sec) AS backup_lcp_speed_last_60sec, SUM(redo_speed_last_60sec) AS redo_speed_last_60sec FROM `ndbinfo`.`ndb$disk_write_speed_aggregate` GROUP by node_id','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_base
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_base` AS SELECT * FROM `ndbinfo`.`ndb$disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.diskpagebuffer
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`diskpagebuffer` AS SELECT node_id, block_instance, pages_written, pages_written_lcp, pages_read, log_waits, page_requests_direct_return, page_requests_wait_queue, page_requests_wait_io FROM `ndbinfo`.`ndb$diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.error_messages
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`error_messages` AS SELECT error_code, error_description, error_status, error_classification FROM `ndbinfo`.`ndb$error_messages`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.locks_per_fragment
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`locks_per_fragment` AS SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, types.type_name AS type, table_id, node_id, block_instance, fragment_num, ex_req, ex_imm_ok, ex_wait_ok, ex_wait_fail, sh_req, sh_imm_ok, sh_wait_ok, sh_wait_fail, wait_ok_millis, wait_fail_millis FROM `ndbinfo`.`ndb$frag_locks` AS locks JOIN `ndbinfo`.`ndb$dict_obj_info` AS name ON name.id=locks.table_id AND name.type<=6 JOIN `ndbinfo`.`ndb$dict_obj_types` AS types ON name.type=types.type_id LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name ON name.parent_obj_id=parent_name.id AND name.parent_obj_type=parent_name.type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.logbuffers
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`logbuffers` AS SELECT node_id,  CASE log_type  WHEN 0 THEN "REDO"  WHEN 1 THEN "DD-UNDO"  WHEN 2 THEN "BACKUP-DATA"  WHEN 3 THEN "BACKUP-LOG"  ELSE "<unknown>"  END AS log_type, log_id, log_part, total, used FROM `ndbinfo`.`ndb$logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.logspaces
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`logspaces` AS SELECT node_id,  CASE log_type  WHEN 0 THEN "REDO"  WHEN 1 THEN "DD-UNDO"  ELSE NULL  END AS log_type, log_id, log_part, total, used FROM `ndbinfo`.`ndb$logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.membership
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`membership` AS SELECT node_id, group_id, left_node, right_node, president, successor, dynamic_id & 0xFFFF AS succession_order, dynamic_id >> 16 AS Conf_HB_order, arbitrator, arb_ticket, CASE arb_state  WHEN 0 THEN "ARBIT_NULL"  WHEN 1 THEN "ARBIT_INIT"  WHEN 2 THEN "ARBIT_FIND"  WHEN 3 THEN "ARBIT_PREP1"  WHEN 4 THEN "ARBIT_PREP2"  WHEN 5 THEN "ARBIT_START"  WHEN 6 THEN "ARBIT_RUN"  WHEN 7 THEN "ARBIT_CHOOSE"  WHEN 8 THEN "ARBIT_CRASH"  ELSE "UNKNOWN" END AS arb_state, CASE arb_connected  WHEN 1 THEN "Yes"  ELSE "No" END AS arb_connected, conn_rank1_arbs AS connected_rank1_arbs, conn_rank2_arbs AS connected_rank2_arbs FROM `ndbinfo`.`ndb$membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.memory_per_fragment
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`memory_per_fragment` AS SELECT name.fq_name, parent_name.fq_name AS parent_fq_name,types.type_name AS type, table_id, node_id, block_instance, fragment_num, fixed_elem_alloc_bytes, fixed_elem_free_bytes, fixed_elem_size_bytes, fixed_elem_count, FLOOR(fixed_elem_free_bytes/fixed_elem_size_bytes) AS fixed_elem_free_count, var_elem_alloc_bytes, var_elem_free_bytes, var_elem_count, hash_index_alloc_bytes FROM `ndbinfo`.`ndb$frag_mem_use` AS space JOIN `ndbinfo`.`ndb$dict_obj_info` AS name ON name.id=space.table_id AND name.type<=6 JOIN  `ndbinfo`.`ndb$dict_obj_types` AS types ON name.type=types.type_id LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name ON name.parent_obj_id=parent_name.id AND name.parent_obj_type=parent_name.type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.memoryusage
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`memoryusage` AS SELECT node_id,  pool_name AS memory_type,  SUM(used*entry_size) AS used,  SUM(used) AS used_pages,  SUM(total*entry_size) AS total,  SUM(total) AS total_pages FROM `ndbinfo`.`ndb$pools` WHERE block_number = 254 GROUP BY node_id, memory_type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.nodes
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`nodes` AS SELECT node_id, uptime, CASE status  WHEN 0 THEN "NOTHING"  WHEN 1 THEN "CMVMI"  WHEN 2 THEN "STARTING"  WHEN 3 THEN "STARTED"  WHEN 4 THEN "SINGLEUSER"  WHEN 5 THEN "STOPPING_1"  WHEN 6 THEN "STOPPING_2"  WHEN 7 THEN "STOPPING_3"  WHEN 8 THEN "STOPPING_4"  ELSE "<unknown>"  END AS status, start_phase, config_generation FROM `ndbinfo`.`ndb$nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.operations_per_fragment
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`operations_per_fragment` AS SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, types.type_name AS type, table_id, node_id, block_instance, fragment_num, tot_key_reads, tot_key_inserts, tot_key_updates, tot_key_writes, tot_key_deletes, tot_key_refs, tot_key_attrinfo_bytes,tot_key_keyinfo_bytes, tot_key_prog_bytes, tot_key_inst_exec, tot_key_bytes_returned, tot_frag_scans, tot_scan_rows_examined, tot_scan_rows_returned, tot_scan_bytes_returned, tot_scan_prog_bytes, tot_scan_bound_bytes, tot_scan_inst_exec, tot_qd_frag_scans, conc_frag_scans,conc_qd_plain_frag_scans+conc_qd_tup_frag_scans+conc_qd_acc_frag_scans AS conc_qd_frag_scans, tot_commits FROM ndbinfo.ndb$frag_operations AS ops JOIN ndbinfo.ndb$dict_obj_info AS name ON name.id=ops.table_id AND name.type<=6 JOIN `ndbinfo`.`ndb$dict_obj_types` AS types ON name.type=types.type_id LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name ON name.parent_obj_id=parent_name.id AND name.parent_obj_type=parent_name.type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.processes
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`processes` AS SELECT DISTINCT node_id, CASE node_type  WHEN 0 THEN "NDB"  WHEN 1 THEN "API"  WHEN 2 THEN "MGM"  ELSE NULL  END AS node_type,  node_version,  NULLIF(process_id, 0) AS process_id,  NULLIF(angel_process_id, 0) AS angel_process_id,  process_name, service_URI FROM `ndbinfo`.`ndb$processes` ORDER BY node_id','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.resources
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`resources` AS SELECT node_id,  CASE resource_id  WHEN 0 THEN "RESERVED"  WHEN 1 THEN "TRANSACTION_MEMORY"  WHEN 2 THEN "DISK_RECORDS"  WHEN 3 THEN "DATA_MEMORY"  WHEN 4 THEN "JOBBUFFER"  WHEN 5 THEN "FILE_BUFFERS"  WHEN 6 THEN "TRANSPORTER_BUFFERS"  WHEN 7 THEN "DISK_PAGE_BUFFER"  WHEN 8 THEN "QUERY_MEMORY"  WHEN 9 THEN "SCHEMA_TRANS_MEMORY"  ELSE "<unknown>"  END AS resource_name, reserved, used, max, spare FROM `ndbinfo`.`ndb$resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.restart_info
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`restart_info` AS SELECT * FROM `ndbinfo`.`ndb$restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.server_locks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`server_locks` AS SELECT map.mysql_connection_id, l.* FROM `ndbinfo`.cluster_locks l JOIN information_schema.ndb_transid_mysql_connection_map map ON (map.ndb_transid >> 32) = (l.transid >> 32)','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.server_operations
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`server_operations` AS SELECT map.mysql_connection_id, o.* FROM `ndbinfo`.cluster_operations o JOIN information_schema.ndb_transid_mysql_connection_map map  ON (map.ndb_transid >> 32) = (o.transid >> 32)','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.server_transactions
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`server_transactions` AS SELECT map.mysql_connection_id, t.*FROM information_schema.ndb_transid_mysql_connection_map map JOIN `ndbinfo`.cluster_transactions t   ON (map.ndb_transid >> 32) = (t.transid >> 32)','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.table_distribution_status
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`table_distribution_status` AS SELECT node_id AS node_id, table_id AS table_id, CASE tab_copy_status WHEN 0 THEN "IDLE" WHEN 1 THEN "SR_PHASE1_READ_PAGES" WHEN 2 THEN "SR_PHASE2_READ_TABLE" WHEN 3 THEN "SR_PHASE3_COPY_TABLE" WHEN 4 THEN "REMOVE_NODE" WHEN 5 THEN "LCP_READ_TABLE" WHEN 6 THEN "COPY_TAB_REQ" WHEN 7 THEN "COPY_NODE_STATE" WHEN 8 THEN "ADD_TABLE_MASTER" WHEN 9 THEN "ADD_TABLE_SLAVE" WHEN 10 THEN "INVALIDATE_NODE_LCP" WHEN 11 THEN "ALTER_TABLE" WHEN 12 THEN "COPY_TO_SAVE" WHEN 13 THEN "GET_TABINFO"  ELSE "Invalid value" END AS tab_copy_status, CASE tab_update_status WHEN 0 THEN "IDLE" WHEN 1 THEN "LOCAL_CHECKPOINT" WHEN 2 THEN "LOCAL_CHECKPOINT_QUEUED" WHEN 3 THEN "REMOVE_NODE" WHEN 4 THEN "COPY_TAB_REQ" WHEN 5 THEN "ADD_TABLE_MASTER" WHEN 6 THEN "ADD_TABLE_SLAVE" WHEN 7 THEN "INVALIDATE_NODE_LCP" WHEN 8 THEN "CALLBACK"  ELSE "Invalid value" END AS tab_update_status, CASE tab_lcp_status WHEN 1 THEN "ACTIVE" WHEN 2 THEN "wRITING_TO_FILE" WHEN 3 THEN "COMPLETED"  ELSE "Invalid value" END AS tab_lcp_status, CASE tab_status WHEN 0 THEN "IDLE" WHEN 1 THEN "ACTIVE" WHEN 2 THEN "CREATING" WHEN 3 THEN "DROPPING"  ELSE "Invalid value" END AS tab_status, CASE tab_storage WHEN 0 THEN "NOLOGGING" WHEN 1 THEN "NORMAL" WHEN 2 THEN "TEMPORARY"  ELSE "Invalid value" END AS tab_storage, tab_partitions AS tab_partitions, tab_fragments AS tab_fragments, current_scan_count AS current_scan_count, scan_count_wait AS scan_count_wait, is_reorg_ongoing AS is_reorg_ongoing FROM `ndbinfo`.`ndb$table_distribution_status`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.table_fragments
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`table_fragments` AS SELECT * FROM `ndbinfo`.`ndb$table_fragments`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.table_info
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`table_info` AS  SELECT  table_id AS table_id,  logged_table AS logged_table,  row_contains_gci AS row_contains_gci,  row_contains_checksum AS row_contains_checksum,  read_backup AS read_backup,  fully_replicated AS fully_replicated,  CASE storage_type WHEN 0 THEN "MEMORY" WHEN 1 THEN "DISK" WHEN 2 THEN "MEMORY"  ELSE "Invalid value" END AS storage_type, hashmap_id AS hashmap_id,  CASE partition_balance WHEN 4294967295 THEN "SPECIFIC" WHEN 4294967294 THEN "FOR_RP_BY_LDM" WHEN 4294967293 THEN "FOR_RA_BY_LDM" WHEN 4294967292 THEN "FOR_RP_BY_NODE" WHEN 4294967291 THEN "FOR_RA_BY_NODE" WHEN 4294967290 THEN "FOR_RA_BY_LDM_X_2" WHEN 4294967289 THEN "FOR_RA_BY_LDM_X_3" WHEN 4294967288 THEN "FOR_RA_BY_LDM_X_4" ELSE "Invalid value" END AS partition_balance, create_gci AS create_gci FROM `ndbinfo`.`ndb$stored_tables`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.table_replicas
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`table_replicas` AS SELECT * FROM `ndbinfo`.`ndb$table_replicas`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.tc_time_track_stats
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`tc_time_track_stats` AS SELECT * FROM `ndbinfo`.`ndb$tc_time_track_stats`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.threadblocks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`threadblocks` AS SELECT t.node_id, t.thr_no, b.block_name, t.block_instance FROM `ndbinfo`.`ndb$threadblocks` t LEFT JOIN `ndbinfo`.`ndb$blocks` b ON t.block_number = b.block_number','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.threads
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`threads` AS SELECT * FROM `ndbinfo`.`ndb$threads`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.threadstat
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`threadstat` AS SELECT * FROM `ndbinfo`.`ndb$threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.transporters
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`transporters` AS SELECT node_id, remote_node_id,  CASE connection_status  WHEN 0 THEN "CONNECTED"  WHEN 1 THEN "CONNECTING"  WHEN 2 THEN "DISCONNECTED"  WHEN 3 THEN "DISCONNECTING"  ELSE NULL  END AS status,  remote_address, bytes_sent, bytes_received,  connect_count,  overloaded, overload_count, slowdown, slowdown_count FROM `ndbinfo`.`ndb$transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Finally turn off offline mode
SET @str=IF(@have_ndbinfo,'SET @@global.ndbinfo_offline=FALSE','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Generated by ndbinfo_sql # DO NOT EDIT! # End

# Restore sql_require_primary_key variable
SET @@session.sql_require_primary_key = @old_sql_require_primary_key;
