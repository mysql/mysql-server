-- Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.
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

--
-- The system tables of MySQL Server
--

set sql_mode='';
set default_storage_engine=myisam;

CREATE TABLE IF NOT EXISTS db (   Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,Db,User), KEY User (User) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='Database privileges';

-- Remember for later if db table already existed
set @had_db_table= @@warning_count != 0;

CREATE TABLE IF NOT EXISTS user (   Host char(60) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL, ssl_cipher BLOB NOT NULL, x509_issuer BLOB NOT NULL, x509_subject BLOB NOT NULL, max_questions int(11) unsigned DEFAULT 0  NOT NULL, max_updates int(11) unsigned DEFAULT 0  NOT NULL, max_connections int(11) unsigned DEFAULT 0  NOT NULL, max_user_connections int(11) unsigned DEFAULT 0  NOT NULL, plugin char(64) DEFAULT 'mysql_native_password' NOT NULL, authentication_string TEXT, password_expired ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, password_last_changed timestamp NULL DEFAULT NULL, password_lifetime smallint unsigned NULL DEFAULT NULL, account_locked ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,User) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='Users and global privileges';

-- Remember for later if user table already existed
set @had_user_table= @@warning_count != 0;


CREATE TABLE IF NOT EXISTS func (  name char(64) binary DEFAULT '' NOT NULL, ret tinyint(1) DEFAULT '0' NOT NULL, dl char(128) DEFAULT '' NOT NULL, type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL, PRIMARY KEY (name) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='User defined functions';


CREATE TABLE IF NOT EXISTS plugin ( name varchar(64) DEFAULT '' NOT NULL, dl varchar(128) DEFAULT '' NOT NULL, PRIMARY KEY (name) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_general_ci comment='MySQL plugins';


CREATE TABLE IF NOT EXISTS servers ( Server_name char(64) NOT NULL DEFAULT '', Host char(64) NOT NULL DEFAULT '', Db char(64) NOT NULL DEFAULT '', Username char(64) NOT NULL DEFAULT '', Password char(64) NOT NULL DEFAULT '', Port INT(4) NOT NULL DEFAULT '0', Socket char(64) NOT NULL DEFAULT '', Wrapper char(64) NOT NULL DEFAULT '', Owner char(64) NOT NULL DEFAULT '', PRIMARY KEY (Server_name)) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='MySQL Foreign Servers table';


CREATE TABLE IF NOT EXISTS tables_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Table privileges';

CREATE TABLE IF NOT EXISTS columns_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Column privileges';


CREATE TABLE IF NOT EXISTS help_topic ( help_topic_id int unsigned not null, name char(64) not null, help_category_id smallint unsigned not null, description text not null, example text not null, url text not null, primary key (help_topic_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help topics';


CREATE TABLE IF NOT EXISTS help_category ( help_category_id smallint unsigned not null, name  char(64) not null, parent_category_id smallint unsigned null, url text not null, primary key (help_category_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help categories';


CREATE TABLE IF NOT EXISTS help_relation ( help_topic_id int unsigned not null, help_keyword_id  int unsigned not null, primary key (help_keyword_id, help_topic_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='keyword-topic relation';


CREATE TABLE IF NOT EXISTS help_keyword (   help_keyword_id  int unsigned not null, name char(64) not null, primary key (help_keyword_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help keywords';


CREATE TABLE IF NOT EXISTS time_zone_name (   Name char(64) NOT NULL, Time_zone_id int unsigned NOT NULL, PRIMARY KEY Name (Name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone names';


CREATE TABLE IF NOT EXISTS time_zone (   Time_zone_id int unsigned NOT NULL auto_increment, Use_leap_seconds enum('Y','N') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY TzId (Time_zone_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zones';


CREATE TABLE IF NOT EXISTS time_zone_transition (   Time_zone_id int unsigned NOT NULL, Transition_time bigint signed NOT NULL, Transition_type_id int unsigned NOT NULL, PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transitions';


CREATE TABLE IF NOT EXISTS time_zone_transition_type (   Time_zone_id int unsigned NOT NULL, Transition_type_id int unsigned NOT NULL, Offset int signed DEFAULT 0 NOT NULL, Is_DST tinyint unsigned DEFAULT 0 NOT NULL, Abbreviation char(8) DEFAULT '' NOT NULL, PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transition types';


CREATE TABLE IF NOT EXISTS time_zone_leap_second (   Transition_time bigint signed NOT NULL, Correction int signed NOT NULL, PRIMARY KEY TranTime (Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Leap seconds information for time zones';


CREATE TABLE IF NOT EXISTS proc (db char(64) collate utf8_bin DEFAULT '' NOT NULL, name char(64) DEFAULT '' NOT NULL, type enum('FUNCTION','PROCEDURE') NOT NULL, specific_name char(64) DEFAULT '' NOT NULL, language enum('SQL') DEFAULT 'SQL' NOT NULL, sql_data_access enum( 'CONTAINS_SQL', 'NO_SQL', 'READS_SQL_DATA', 'MODIFIES_SQL_DATA') DEFAULT 'CONTAINS_SQL' NOT NULL, is_deterministic enum('YES','NO') DEFAULT 'NO' NOT NULL, security_type enum('INVOKER','DEFINER') DEFAULT 'DEFINER' NOT NULL, param_list blob NOT NULL, returns longblob DEFAULT '' NOT NULL, body longblob NOT NULL, definer char(93) collate utf8_bin DEFAULT '' NOT NULL, created timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, modified timestamp NOT NULL DEFAULT '0000-00-00 00:00:00', sql_mode set( 'REAL_AS_FLOAT', 'PIPES_AS_CONCAT', 'ANSI_QUOTES', 'IGNORE_SPACE', 'NOT_USED', 'ONLY_FULL_GROUP_BY', 'NO_UNSIGNED_SUBTRACTION', 'NO_DIR_IN_CREATE', 'POSTGRESQL', 'ORACLE', 'MSSQL', 'DB2', 'MAXDB', 'NO_KEY_OPTIONS', 'NO_TABLE_OPTIONS', 'NO_FIELD_OPTIONS', 'MYSQL323', 'MYSQL40', 'ANSI', 'NO_AUTO_VALUE_ON_ZERO', 'NO_BACKSLASH_ESCAPES', 'STRICT_TRANS_TABLES', 'STRICT_ALL_TABLES', 'NO_ZERO_IN_DATE', 'NO_ZERO_DATE', 'INVALID_DATES', 'ERROR_FOR_DIVISION_BY_ZERO', 'TRADITIONAL', 'NO_AUTO_CREATE_USER', 'HIGH_NOT_PRECEDENCE', 'NO_ENGINE_SUBSTITUTION', 'PAD_CHAR_TO_FULL_LENGTH') DEFAULT '' NOT NULL, comment text collate utf8_bin NOT NULL, character_set_client char(32) collate utf8_bin, collation_connection char(32) collate utf8_bin, db_collation char(32) collate utf8_bin, body_utf8 longblob, PRIMARY KEY (db,name,type)) engine=MyISAM character set utf8 comment='Stored Procedures';

CREATE TABLE IF NOT EXISTS procs_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Routine_name char(64) COLLATE utf8_general_ci DEFAULT '' NOT NULL, Routine_type enum('FUNCTION','PROCEDURE') NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY (Host,Db,User,Routine_name,Routine_type), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Procedure privileges';

-- Create general_log
CREATE TABLE IF NOT EXISTS general_log (event_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL, server_id INTEGER UNSIGNED NOT NULL, command_type VARCHAR(64) NOT NULL, argument MEDIUMBLOB NOT NULL) engine=CSV CHARACTER SET utf8 comment="General log";

-- Create slow_log
CREATE TABLE IF NOT EXISTS slow_log (start_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, query_time TIME(6) NOT NULL, lock_time TIME(6) NOT NULL, rows_sent INTEGER NOT NULL, rows_examined INTEGER NOT NULL, db VARCHAR(512) NOT NULL, last_insert_id INTEGER NOT NULL, insert_id INTEGER NOT NULL, server_id INTEGER UNSIGNED NOT NULL, sql_text MEDIUMBLOB NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL) engine=CSV CHARACTER SET utf8 comment="Slow log";

CREATE TABLE IF NOT EXISTS event ( db char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', name char(64) CHARACTER SET utf8 NOT NULL default '', body longblob NOT NULL, definer char(93) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', execute_at DATETIME default NULL, interval_value int(11) default NULL, interval_field ENUM('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND','DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND','SECOND_MICROSECOND') default NULL, created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, modified TIMESTAMP NOT NULL DEFAULT '0000-00-00 00:00:00', last_executed DATETIME default NULL, starts DATETIME default NULL, ends DATETIME default NULL, status ENUM('ENABLED','DISABLED','SLAVESIDE_DISABLED') NOT NULL default 'ENABLED', on_completion ENUM('DROP','PRESERVE') NOT NULL default 'DROP', sql_mode  set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB','NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40','ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH') DEFAULT '' NOT NULL, comment char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', originator INTEGER UNSIGNED NOT NULL, time_zone char(64) CHARACTER SET latin1 NOT NULL DEFAULT 'SYSTEM', character_set_client char(32) collate utf8_bin, collation_connection char(32) collate utf8_bin, db_collation char(32) collate utf8_bin, body_utf8 longblob, PRIMARY KEY (db, name) ) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT 'Events';


CREATE TABLE IF NOT EXISTS ndb_binlog_index (Position BIGINT UNSIGNED NOT NULL, File VARCHAR(255) NOT NULL, epoch BIGINT UNSIGNED NOT NULL, inserts INT UNSIGNED NOT NULL, updates INT UNSIGNED NOT NULL, deletes INT UNSIGNED NOT NULL, schemaops INT UNSIGNED NOT NULL, orig_server_id INT UNSIGNED NOT NULL, orig_epoch BIGINT UNSIGNED NOT NULL, gci INT UNSIGNED NOT NULL, next_position BIGINT UNSIGNED NOT NULL, next_file VARCHAR(255) NOT NULL, PRIMARY KEY(epoch, orig_server_id, orig_epoch)) ENGINE=MYISAM;

SET @sql_mode_orig=@@SESSION.sql_mode;
SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

SET @create_innodb_table_stats="CREATE TABLE IF NOT EXISTS innodb_table_stats (
	database_name			VARCHAR(64) NOT NULL,
	table_name			VARCHAR(64) NOT NULL,
	last_update			TIMESTAMP NOT NULL NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	n_rows				BIGINT UNSIGNED NOT NULL,
	clustered_index_size		BIGINT UNSIGNED NOT NULL,
	sum_of_other_index_sizes	BIGINT UNSIGNED NOT NULL,
	PRIMARY KEY (database_name, table_name)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COLLATE=utf8_bin STATS_PERSISTENT=0";

SET @create_innodb_index_stats="CREATE TABLE IF NOT EXISTS innodb_index_stats (
	database_name			VARCHAR(64) NOT NULL,
	table_name			VARCHAR(64) NOT NULL,
	index_name			VARCHAR(64) NOT NULL,
	last_update			TIMESTAMP NOT NULL NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	/* there are at least:
	stat_name='size'
	stat_name='n_leaf_pages'
	stat_name='n_diff_pfx%' */
	stat_name			VARCHAR(64) NOT NULL,
	stat_value			BIGINT UNSIGNED NOT NULL,
	sample_size			BIGINT UNSIGNED,
	stat_description		VARCHAR(1024) NOT NULL,
	PRIMARY KEY (database_name, table_name, index_name, stat_name)
) ENGINE=INNODB DEFAULT CHARSET=utf8 COLLATE=utf8_bin STATS_PERSISTENT=0";

set @have_innodb= (select count(engine) from information_schema.engines where engine='INNODB' and support != 'NO');

SET @str=IF(@have_innodb <> 0, @create_innodb_table_stats, "SET @dummy = 0");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_innodb <> 0, @create_innodb_index_stats, "SET @dummy = 0");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET SESSION sql_mode=@sql_mode_orig;

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

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB;'), CONCAT(@cmd, ' ENGINE= MYISAM;'));
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
  PRIMARY KEY(Channel_name)) DEFAULT CHARSET=utf8 STATS_PERSISTENT=0 COMMENT 'Master Information'";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB;'), CONCAT(@cmd, ' ENGINE= MYISAM;'));
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

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB;'), CONCAT(@cmd, ' ENGINE= MYISAM;'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE TABLE IF NOT EXISTS gtid_executed (
    source_uuid CHAR(36) NOT NULL COMMENT 'uuid of the source where the transaction was originally executed.',
    interval_start BIGINT NOT NULL COMMENT 'First number of interval.',
    interval_end BIGINT NOT NULL COMMENT 'Last number of interval.',
    PRIMARY KEY(source_uuid, interval_start))";

SET @str=IF(@have_innodb <> 0, CONCAT(@cmd, ' ENGINE= INNODB;'), CONCAT(@cmd, ' ENGINE= MYISAM;'));
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- Optimizer Cost Model configuration
--

-- Server cost constants

CREATE TABLE IF NOT EXISTS server_cost (
  cost_name   VARCHAR(64) NOT NULL,
  cost_value  FLOAT DEFAULT NULL,
  last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  comment     VARCHAR(1024) DEFAULT NULL,
  PRIMARY KEY (cost_name)
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci STATS_PERSISTENT=0;

INSERT IGNORE INTO server_cost(cost_name) VALUES
  ("row_evaluate_cost"), ("key_compare_cost"),
  ("memory_temptable_create_cost"), ("memory_temptable_row_cost"),
  ("disk_temptable_create_cost"), ("disk_temptable_row_cost");

-- Engine cost constants

CREATE TABLE IF NOT EXISTS engine_cost (
  engine_name VARCHAR(64) NOT NULL,
  device_type INTEGER NOT NULL,
  cost_name   VARCHAR(64) NOT NULL,
  cost_value  FLOAT DEFAULT NULL,
  last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  comment     VARCHAR(1024) DEFAULT NULL,
  PRIMARY KEY (cost_name, engine_name, device_type)
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_general_ci STATS_PERSISTENT=0;

INSERT IGNORE INTO engine_cost(engine_name, device_type, cost_name) VALUES
  ("default", 0, "memory_block_read_cost"),
  ("default", 0, "io_block_read_cost");

--
-- PERFORMANCE SCHEMA INSTALLATION
-- Note that this script is also reused by mysql_upgrade,
-- so we have to be very careful here to not destroy any
-- existing database named 'performance_schema' if it
-- can contain user data.
-- In case of downgrade, it's ok to drop unknown tables
-- from a future version, as long as they belong to the
-- performance schema engine.
--

set @have_old_pfs= (select count(*) from information_schema.schemata where schema_name='performance_schema');

SET @cmd="SET @broken_tables = (select count(*) from information_schema.tables"
  " where engine != \'PERFORMANCE_SCHEMA\' and table_schema=\'performance_schema\')";

-- Work around for bug#49542
SET @str = IF(@have_old_pfs = 1, @cmd, 'SET @broken_tables = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="SET @broken_views = (select count(*) from information_schema.views"
  " where table_schema='performance_schema')";

-- Work around for bug#49542
SET @str = IF(@have_old_pfs = 1, @cmd, 'SET @broken_views = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @broken_routines = (select count(*) from mysql.proc where db='performance_schema');

SET @broken_events = (select count(*) from mysql.event where db='performance_schema');

SET @broken_pfs= (select @broken_tables + @broken_views + @broken_routines + @broken_events);

--
-- The performance schema database.
-- Only drop and create the database if this is safe (no broken_pfs).
-- This database is created, even in --without-perfschema builds,
-- so that the database name is always reserved by the MySQL implementation.
--

SET @cmd= "DROP DATABASE IF EXISTS performance_schema";

SET @str = IF(@broken_pfs = 0, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd= "CREATE DATABASE performance_schema character set utf8";

SET @str = IF(@broken_pfs = 0, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- From this point, only create the performance schema tables
-- if the server is built with performance schema
--

set @have_pfs= (select count(engine) from information_schema.engines where engine='PERFORMANCE_SCHEMA');

--
-- TABLE COND_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.cond_instances("
  "NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_CURRENT
--

SET @cmd="CREATE TABLE performance_schema.events_waits_current("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "SPINS INTEGER unsigned,"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(512),"
  "INDEX_NAME VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "OPERATION VARCHAR(32) not null,"
  "NUMBER_OF_BYTES BIGINT,"
  "FLAGS INTEGER unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_waits_history("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "SPINS INTEGER unsigned,"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(512),"
  "INDEX_NAME VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "OPERATION VARCHAR(32) not null,"
  "NUMBER_OF_BYTES BIGINT,"
  "FLAGS INTEGER unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_waits_history_long("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "SPINS INTEGER unsigned,"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(512),"
  "INDEX_NAME VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "OPERATION VARCHAR(32) not null,"
  "NUMBER_OF_BYTES BIGINT,"
  "FLAGS INTEGER unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_INSTANCE
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_by_instance("
  "EVENT_NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_HOST_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_by_host_by_event_name("
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_by_user_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_by_account_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_by_thread_by_event_name("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_WAITS_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_waits_summary_global_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.file_instances("
  "FILE_NAME VARCHAR(512) not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "OPEN_COUNT INTEGER unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_SUMMARY_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.file_summary_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_READ BIGINT not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_WRITE BIGINT not null,"
  "COUNT_MISC BIGINT unsigned not null,"
  "SUM_TIMER_MISC BIGINT unsigned not null,"
  "MIN_TIMER_MISC BIGINT unsigned not null,"
  "AVG_TIMER_MISC BIGINT unsigned not null,"
  "MAX_TIMER_MISC BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE FILE_SUMMARY_BY_INSTANCE
--

SET @cmd="CREATE TABLE performance_schema.file_summary_by_instance("
  "FILE_NAME VARCHAR(512) not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_READ BIGINT not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_WRITE BIGINT not null,"
  "COUNT_MISC BIGINT unsigned not null,"
  "SUM_TIMER_MISC BIGINT unsigned not null,"
  "MIN_TIMER_MISC BIGINT unsigned not null,"
  "AVG_TIMER_MISC BIGINT unsigned not null,"
  "MAX_TIMER_MISC BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- TABLE SOCKET_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.socket_instances("
  "EVENT_NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "THREAD_ID BIGINT unsigned,"
  "SOCKET_ID INTEGER not null,"
  "IP VARCHAR(64) not null,"
  "PORT INTEGER not null,"
  "STATE ENUM('IDLE','ACTIVE') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SOCKET_SUMMARY_BY_INSTANCE
--

SET @cmd="CREATE TABLE performance_schema.socket_summary_by_instance("
  "EVENT_NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_READ BIGINT unsigned not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_WRITE BIGINT unsigned not null,"
  "COUNT_MISC BIGINT unsigned not null,"
  "SUM_TIMER_MISC BIGINT unsigned not null,"
  "MIN_TIMER_MISC BIGINT unsigned not null,"
  "AVG_TIMER_MISC BIGINT unsigned not null,"
  "MAX_TIMER_MISC BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SOCKET_SUMMARY_BY_INSTANCE
--

SET @cmd="CREATE TABLE performance_schema.socket_summary_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_READ BIGINT unsigned not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_WRITE BIGINT unsigned not null,"
  "COUNT_MISC BIGINT unsigned not null,"
  "SUM_TIMER_MISC BIGINT unsigned not null,"
  "MIN_TIMER_MISC BIGINT unsigned not null,"
  "AVG_TIMER_MISC BIGINT unsigned not null,"
  "MAX_TIMER_MISC BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE HOST_CACHE
--

SET @cmd="CREATE TABLE performance_schema.host_cache("
  "IP VARCHAR(64) not null,"
  "HOST VARCHAR(255) collate utf8_bin,"
  "HOST_VALIDATED ENUM ('YES', 'NO') not null,"
  "SUM_CONNECT_ERRORS BIGINT not null,"
  "COUNT_HOST_BLOCKED_ERRORS BIGINT not null,"
  "COUNT_NAMEINFO_TRANSIENT_ERRORS BIGINT not null,"
  "COUNT_NAMEINFO_PERMANENT_ERRORS BIGINT not null,"
  "COUNT_FORMAT_ERRORS BIGINT not null,"
  "COUNT_ADDRINFO_TRANSIENT_ERRORS BIGINT not null,"
  "COUNT_ADDRINFO_PERMANENT_ERRORS BIGINT not null,"
  "COUNT_FCRDNS_ERRORS BIGINT not null,"
  "COUNT_HOST_ACL_ERRORS BIGINT not null,"
  "COUNT_NO_AUTH_PLUGIN_ERRORS BIGINT not null,"
  "COUNT_AUTH_PLUGIN_ERRORS BIGINT not null,"
  "COUNT_HANDSHAKE_ERRORS BIGINT not null,"
  "COUNT_PROXY_USER_ERRORS BIGINT not null,"
  "COUNT_PROXY_USER_ACL_ERRORS BIGINT not null,"
  "COUNT_AUTHENTICATION_ERRORS BIGINT not null,"
  "COUNT_SSL_ERRORS BIGINT not null,"
  "COUNT_MAX_USER_CONNECTIONS_ERRORS BIGINT not null,"
  "COUNT_MAX_USER_CONNECTIONS_PER_HOUR_ERRORS BIGINT not null,"
  "COUNT_DEFAULT_DATABASE_ERRORS BIGINT not null,"
  "COUNT_INIT_CONNECT_ERRORS BIGINT not null,"
  "COUNT_LOCAL_ERRORS BIGINT not null,"
  "COUNT_UNKNOWN_ERRORS BIGINT not null,"
  "FIRST_SEEN TIMESTAMP(0) NOT NULL default 0,"
  "LAST_SEEN TIMESTAMP(0) NOT NULL default 0,"
  "FIRST_ERROR_SEEN TIMESTAMP(0) null default 0,"
  "LAST_ERROR_SEEN TIMESTAMP(0) null default 0"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MUTEX_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.mutex_instances("
  "NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "LOCKED_BY_THREAD_ID BIGINT unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE OBJECTS_SUMMARY_GLOBAL_BY_TYPE
--

SET @cmd="CREATE TABLE performance_schema.objects_summary_global_by_type("
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE PERFORMANCE_TIMERS
--

SET @cmd="CREATE TABLE performance_schema.performance_timers("
  "TIMER_NAME ENUM ('CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'TICK') not null,"
  "TIMER_FREQUENCY BIGINT,"
  "TIMER_RESOLUTION BIGINT,"
  "TIMER_OVERHEAD BIGINT"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE RWLOCK_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.rwlock_instances("
  "NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "WRITE_LOCKED_BY_THREAD_ID BIGINT unsigned,"
  "READ_LOCKED_BY_COUNT INTEGER unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_ACTORS
--

SET @cmd="CREATE TABLE performance_schema.setup_actors("
  "HOST CHAR(60) collate utf8_bin default '%' not null,"
  "USER CHAR(32) collate utf8_bin default '%' not null,"
  "ROLE CHAR(16) collate utf8_bin default '%' not null,"
  "ENABLED ENUM ('YES', 'NO') not null default 'YES',"
  "HISTORY ENUM ('YES', 'NO') not null default 'YES'"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_CONSUMERS
--

SET @cmd="CREATE TABLE performance_schema.setup_consumers("
  "NAME VARCHAR(64) not null,"
  "ENABLED ENUM ('YES', 'NO') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_INSTRUMENTS
--

SET @cmd="CREATE TABLE performance_schema.setup_instruments("
  "NAME VARCHAR(128) not null,"
  "ENABLED ENUM ('YES', 'NO') not null,"
  "TIMED ENUM ('YES', 'NO') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_OBJECTS
--

SET @cmd="CREATE TABLE performance_schema.setup_objects("
  "OBJECT_TYPE ENUM ('EVENT', 'FUNCTION', 'PROCEDURE', 'TABLE', 'TRIGGER') not null default 'TABLE',"
  "OBJECT_SCHEMA VARCHAR(64) default '%',"
  "OBJECT_NAME VARCHAR(64) not null default '%',"
  "ENABLED ENUM ('YES', 'NO') not null default 'YES',"
  "TIMED ENUM ('YES', 'NO') not null default 'YES'"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SETUP_TIMERS
--

SET @cmd="CREATE TABLE performance_schema.setup_timers("
  "NAME VARCHAR(64) not null,"
  "TIMER_NAME ENUM ('CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'TICK') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE TABLE_IO_WAITS_SUMMARY_BY_INDEX_USAGE
--

SET @cmd="CREATE TABLE performance_schema.table_io_waits_summary_by_index_usage("
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "INDEX_NAME VARCHAR(64),"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "COUNT_FETCH BIGINT unsigned not null,"
  "SUM_TIMER_FETCH BIGINT unsigned not null,"
  "MIN_TIMER_FETCH BIGINT unsigned not null,"
  "AVG_TIMER_FETCH BIGINT unsigned not null,"
  "MAX_TIMER_FETCH BIGINT unsigned not null,"
  "COUNT_INSERT BIGINT unsigned not null,"
  "SUM_TIMER_INSERT BIGINT unsigned not null,"
  "MIN_TIMER_INSERT BIGINT unsigned not null,"
  "AVG_TIMER_INSERT BIGINT unsigned not null,"
  "MAX_TIMER_INSERT BIGINT unsigned not null,"
  "COUNT_UPDATE BIGINT unsigned not null,"
  "SUM_TIMER_UPDATE BIGINT unsigned not null,"
  "MIN_TIMER_UPDATE BIGINT unsigned not null,"
  "AVG_TIMER_UPDATE BIGINT unsigned not null,"
  "MAX_TIMER_UPDATE BIGINT unsigned not null,"
  "COUNT_DELETE BIGINT unsigned not null,"
  "SUM_TIMER_DELETE BIGINT unsigned not null,"
  "MIN_TIMER_DELETE BIGINT unsigned not null,"
  "AVG_TIMER_DELETE BIGINT unsigned not null,"
  "MAX_TIMER_DELETE BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE TABLE_IO_WAITS_SUMMARY_BY_TABLE
--

SET @cmd="CREATE TABLE performance_schema.table_io_waits_summary_by_table("
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "COUNT_FETCH BIGINT unsigned not null,"
  "SUM_TIMER_FETCH BIGINT unsigned not null,"
  "MIN_TIMER_FETCH BIGINT unsigned not null,"
  "AVG_TIMER_FETCH BIGINT unsigned not null,"
  "MAX_TIMER_FETCH BIGINT unsigned not null,"
  "COUNT_INSERT BIGINT unsigned not null,"
  "SUM_TIMER_INSERT BIGINT unsigned not null,"
  "MIN_TIMER_INSERT BIGINT unsigned not null,"
  "AVG_TIMER_INSERT BIGINT unsigned not null,"
  "MAX_TIMER_INSERT BIGINT unsigned not null,"
  "COUNT_UPDATE BIGINT unsigned not null,"
  "SUM_TIMER_UPDATE BIGINT unsigned not null,"
  "MIN_TIMER_UPDATE BIGINT unsigned not null,"
  "AVG_TIMER_UPDATE BIGINT unsigned not null,"
  "MAX_TIMER_UPDATE BIGINT unsigned not null,"
  "COUNT_DELETE BIGINT unsigned not null,"
  "SUM_TIMER_DELETE BIGINT unsigned not null,"
  "MIN_TIMER_DELETE BIGINT unsigned not null,"
  "AVG_TIMER_DELETE BIGINT unsigned not null,"
  "MAX_TIMER_DELETE BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE TABLE_LOCK_WAITS_SUMMARY_BY_TABLE
--

SET @cmd="CREATE TABLE performance_schema.table_lock_waits_summary_by_table("
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ BIGINT unsigned not null,"
  "SUM_TIMER_READ BIGINT unsigned not null,"
  "MIN_TIMER_READ BIGINT unsigned not null,"
  "AVG_TIMER_READ BIGINT unsigned not null,"
  "MAX_TIMER_READ BIGINT unsigned not null,"
  "COUNT_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE BIGINT unsigned not null,"
  "COUNT_READ_NORMAL BIGINT unsigned not null,"
  "SUM_TIMER_READ_NORMAL BIGINT unsigned not null,"
  "MIN_TIMER_READ_NORMAL BIGINT unsigned not null,"
  "AVG_TIMER_READ_NORMAL BIGINT unsigned not null,"
  "MAX_TIMER_READ_NORMAL BIGINT unsigned not null,"
  "COUNT_READ_WITH_SHARED_LOCKS BIGINT unsigned not null,"
  "SUM_TIMER_READ_WITH_SHARED_LOCKS BIGINT unsigned not null,"
  "MIN_TIMER_READ_WITH_SHARED_LOCKS BIGINT unsigned not null,"
  "AVG_TIMER_READ_WITH_SHARED_LOCKS BIGINT unsigned not null,"
  "MAX_TIMER_READ_WITH_SHARED_LOCKS BIGINT unsigned not null,"
  "COUNT_READ_HIGH_PRIORITY BIGINT unsigned not null,"
  "SUM_TIMER_READ_HIGH_PRIORITY BIGINT unsigned not null,"
  "MIN_TIMER_READ_HIGH_PRIORITY BIGINT unsigned not null,"
  "AVG_TIMER_READ_HIGH_PRIORITY BIGINT unsigned not null,"
  "MAX_TIMER_READ_HIGH_PRIORITY BIGINT unsigned not null,"
  "COUNT_READ_NO_INSERT BIGINT unsigned not null,"
  "SUM_TIMER_READ_NO_INSERT BIGINT unsigned not null,"
  "MIN_TIMER_READ_NO_INSERT BIGINT unsigned not null,"
  "AVG_TIMER_READ_NO_INSERT BIGINT unsigned not null,"
  "MAX_TIMER_READ_NO_INSERT BIGINT unsigned not null,"
  "COUNT_READ_EXTERNAL BIGINT unsigned not null,"
  "SUM_TIMER_READ_EXTERNAL BIGINT unsigned not null,"
  "MIN_TIMER_READ_EXTERNAL BIGINT unsigned not null,"
  "AVG_TIMER_READ_EXTERNAL BIGINT unsigned not null,"
  "MAX_TIMER_READ_EXTERNAL BIGINT unsigned not null,"
  "COUNT_WRITE_ALLOW_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_ALLOW_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_ALLOW_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_ALLOW_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_ALLOW_WRITE BIGINT unsigned not null,"
  "COUNT_WRITE_CONCURRENT_INSERT BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_CONCURRENT_INSERT BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_CONCURRENT_INSERT BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_CONCURRENT_INSERT BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_CONCURRENT_INSERT BIGINT unsigned not null,"
  "COUNT_WRITE_LOW_PRIORITY BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_LOW_PRIORITY BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_LOW_PRIORITY BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_LOW_PRIORITY BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_LOW_PRIORITY BIGINT unsigned not null,"
  "COUNT_WRITE_NORMAL BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_NORMAL BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_NORMAL BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_NORMAL BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_NORMAL BIGINT unsigned not null,"
  "COUNT_WRITE_EXTERNAL BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_EXTERNAL BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_EXTERNAL BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_EXTERNAL BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_EXTERNAL BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE THREADS
--

SET @cmd="CREATE TABLE performance_schema.threads("
  "THREAD_ID BIGINT unsigned not null,"
  "NAME VARCHAR(128) not null,"
  "TYPE VARCHAR(10) not null,"
  "PROCESSLIST_ID BIGINT unsigned,"
  "PROCESSLIST_USER VARCHAR(32),"
  "PROCESSLIST_HOST VARCHAR(60),"
  "PROCESSLIST_DB VARCHAR(64),"
  "PROCESSLIST_COMMAND VARCHAR(16),"
  "PROCESSLIST_TIME BIGINT,"
  "PROCESSLIST_STATE VARCHAR(64),"
  "PROCESSLIST_INFO LONGTEXT,"
  "PARENT_THREAD_ID BIGINT unsigned,"
  "ROLE VARCHAR(64),"
  "INSTRUMENTED ENUM ('YES', 'NO') not null,"
  "HISTORY ENUM ('YES', 'NO') not null,"
  "CONNECTION_TYPE VARCHAR(16),"
  "THREAD_OS_ID BIGINT unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_CURRENT
--

SET @cmd="CREATE TABLE performance_schema.events_stages_current("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "WORK_COMPLETED BIGINT unsigned,"
  "WORK_ESTIMATED BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_stages_history("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "WORK_COMPLETED BIGINT unsigned,"
  "WORK_ESTIMATED BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_stages_history_long("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "WORK_COMPLETED BIGINT unsigned,"
  "WORK_ESTIMATED BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_by_thread_by_event_name("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_HOST_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_by_host_by_event_name("
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_by_user_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_by_account_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_global_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_CURRENT
--

SET @cmd="CREATE TABLE performance_schema.events_statements_current("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
  "DIGEST VARCHAR(32),"
  "DIGEST_TEXT LONGTEXT,"
  "CURRENT_SCHEMA VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "MYSQL_ERRNO INTEGER,"
  "RETURNED_SQLSTATE VARCHAR(5),"
  "MESSAGE_TEXT VARCHAR(128),"
  "ERRORS BIGINT unsigned not null,"
  "WARNINGS BIGINT unsigned not null,"
  "ROWS_AFFECTED BIGINT unsigned not null,"
  "ROWS_SENT BIGINT unsigned not null,"
  "ROWS_EXAMINED BIGINT unsigned not null,"
  "CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SELECT_RANGE BIGINT unsigned not null,"
  "SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SELECT_SCAN BIGINT unsigned not null,"
  "SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SORT_RANGE BIGINT unsigned not null,"
  "SORT_ROWS BIGINT unsigned not null,"
  "SORT_SCAN BIGINT unsigned not null,"
  "NO_INDEX_USED BIGINT unsigned not null,"
  "NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "NESTING_EVENT_LEVEL INTEGER"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_statements_history("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
  "DIGEST VARCHAR(32),"
  "DIGEST_TEXT LONGTEXT,"
  "CURRENT_SCHEMA VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "MYSQL_ERRNO INTEGER,"
  "RETURNED_SQLSTATE VARCHAR(5),"
  "MESSAGE_TEXT VARCHAR(128),"
  "ERRORS BIGINT unsigned not null,"
  "WARNINGS BIGINT unsigned not null,"
  "ROWS_AFFECTED BIGINT unsigned not null,"
  "ROWS_SENT BIGINT unsigned not null,"
  "ROWS_EXAMINED BIGINT unsigned not null,"
  "CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SELECT_RANGE BIGINT unsigned not null,"
  "SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SELECT_SCAN BIGINT unsigned not null,"
  "SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SORT_RANGE BIGINT unsigned not null,"
  "SORT_ROWS BIGINT unsigned not null,"
  "SORT_SCAN BIGINT unsigned not null,"
  "NO_INDEX_USED BIGINT unsigned not null,"
  "NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "NESTING_EVENT_LEVEL INTEGER"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_statements_history_long("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
  "DIGEST VARCHAR(32),"
  "DIGEST_TEXT LONGTEXT,"
  "CURRENT_SCHEMA VARCHAR(64),"
  "OBJECT_TYPE VARCHAR(64),"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "MYSQL_ERRNO INTEGER,"
  "RETURNED_SQLSTATE VARCHAR(5),"
  "MESSAGE_TEXT VARCHAR(128),"
  "ERRORS BIGINT unsigned not null,"
  "WARNINGS BIGINT unsigned not null,"
  "ROWS_AFFECTED BIGINT unsigned not null,"
  "ROWS_SENT BIGINT unsigned not null,"
  "ROWS_EXAMINED BIGINT unsigned not null,"
  "CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SELECT_RANGE BIGINT unsigned not null,"
  "SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SELECT_SCAN BIGINT unsigned not null,"
  "SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SORT_RANGE BIGINT unsigned not null,"
  "SORT_ROWS BIGINT unsigned not null,"
  "SORT_SCAN BIGINT unsigned not null,"
  "NO_INDEX_USED BIGINT unsigned not null,"
  "NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "NESTING_EVENT_LEVEL INTEGER"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_thread_by_event_name("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_HOST_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_host_by_event_name("
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_user_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_account_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_global_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_CURRENT
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_current("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "STATE ENUM('ACTIVE', 'COMMITTED', 'ROLLED BACK'),"
  "TRX_ID BIGINT unsigned,"
  "GTID VARCHAR(64),"
  "XID_FORMAT_ID INTEGER,"
  "XID_GTRID VARCHAR(130),"
  "XID_BQUAL VARCHAR(130),"
  "XA_STATE VARCHAR(64),"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "ACCESS_MODE ENUM('READ ONLY', 'READ WRITE'),"
  "ISOLATION_LEVEL VARCHAR(64),"
  "AUTOCOMMIT ENUM('YES','NO') not null,"
  "NUMBER_OF_SAVEPOINTS BIGINT unsigned,"
  "NUMBER_OF_ROLLBACK_TO_SAVEPOINT BIGINT unsigned,"
  "NUMBER_OF_RELEASE_SAVEPOINT BIGINT unsigned,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_history("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "STATE ENUM('ACTIVE', 'COMMITTED', 'ROLLED BACK'),"
  "TRX_ID BIGINT unsigned,"
  "GTID VARCHAR(64),"
  "XID_FORMAT_ID INTEGER,"
  "XID_GTRID VARCHAR(130),"
  "XID_BQUAL VARCHAR(130),"
  "XA_STATE VARCHAR(64),"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "ACCESS_MODE ENUM('READ ONLY', 'READ WRITE'),"
  "ISOLATION_LEVEL VARCHAR(64),"
  "AUTOCOMMIT ENUM('YES','NO') not null,"
  "NUMBER_OF_SAVEPOINTS BIGINT unsigned,"
  "NUMBER_OF_ROLLBACK_TO_SAVEPOINT BIGINT unsigned,"
  "NUMBER_OF_RELEASE_SAVEPOINT BIGINT unsigned,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_history_long("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "STATE ENUM('ACTIVE', 'COMMITTED', 'ROLLED BACK'),"
  "TRX_ID BIGINT unsigned,"
  "GTID VARCHAR(64),"
  "XID_FORMAT_ID INTEGER,"
  "XID_GTRID VARCHAR(130),"
  "XID_BQUAL VARCHAR(130),"
  "XA_STATE VARCHAR(64),"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "ACCESS_MODE ENUM('READ ONLY', 'READ WRITE'),"
  "ISOLATION_LEVEL VARCHAR(64),"
  "AUTOCOMMIT ENUM('YES','NO') not null,"
  "NUMBER_OF_SAVEPOINTS BIGINT unsigned,"
  "NUMBER_OF_ROLLBACK_TO_SAVEPOINT BIGINT unsigned,"
  "NUMBER_OF_RELEASE_SAVEPOINT BIGINT unsigned,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_thread_by_event_name("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_READ_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_READ_WRITE BIGINT unsigned not null,"
  "COUNT_READ_ONLY BIGINT unsigned not null,"
  "SUM_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MIN_TIMER_READ_ONLY BIGINT unsigned not null,"
  "AVG_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_BY_HOST_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_host_by_event_name("
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_READ_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_READ_WRITE BIGINT unsigned not null,"
  "COUNT_READ_ONLY BIGINT unsigned not null,"
  "SUM_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MIN_TIMER_READ_ONLY BIGINT unsigned not null,"
  "AVG_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_user_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_READ_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_READ_WRITE BIGINT unsigned not null,"
  "COUNT_READ_ONLY BIGINT unsigned not null,"
  "SUM_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MIN_TIMER_READ_ONLY BIGINT unsigned not null,"
  "AVG_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_account_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_READ_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_READ_WRITE BIGINT unsigned not null,"
  "COUNT_READ_ONLY BIGINT unsigned not null,"
  "SUM_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MIN_TIMER_READ_ONLY BIGINT unsigned not null,"
  "AVG_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_global_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "COUNT_READ_WRITE BIGINT unsigned not null,"
  "SUM_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MIN_TIMER_READ_WRITE BIGINT unsigned not null,"
  "AVG_TIMER_READ_WRITE BIGINT unsigned not null,"
  "MAX_TIMER_READ_WRITE BIGINT unsigned not null,"
  "COUNT_READ_ONLY BIGINT unsigned not null,"
  "SUM_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MIN_TIMER_READ_ONLY BIGINT unsigned not null,"
  "AVG_TIMER_READ_ONLY BIGINT unsigned not null,"
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE HOSTS
--

SET @cmd="CREATE TABLE performance_schema.hosts("
  "HOST CHAR(60) collate utf8_bin default null,"
  "CURRENT_CONNECTIONS bigint not null,"
  "TOTAL_CONNECTIONS bigint not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE USERS
--

SET @cmd="CREATE TABLE performance_schema.users("
  "USER CHAR(32) collate utf8_bin default null,"
  "CURRENT_CONNECTIONS bigint not null,"
  "TOTAL_CONNECTIONS bigint not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE ACCOUNTS
--

SET @cmd="CREATE TABLE performance_schema.accounts("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "CURRENT_CONNECTIONS bigint not null,"
  "TOTAL_CONNECTIONS bigint not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_GLOBAL_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.memory_summary_global_by_event_name("
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_ALLOC BIGINT unsigned not null,"
  "COUNT_FREE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,"
  "LOW_COUNT_USED BIGINT not null,"
  "CURRENT_COUNT_USED BIGINT not null,"
  "HIGH_COUNT_USED BIGINT not null,"
  "LOW_NUMBER_OF_BYTES_USED BIGINT not null,"
  "CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,"
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.memory_summary_by_thread_by_event_name("
  "THREAD_ID BIGINT unsigned not null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_ALLOC BIGINT unsigned not null,"
  "COUNT_FREE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,"
  "LOW_COUNT_USED BIGINT not null,"
  "CURRENT_COUNT_USED BIGINT not null,"
  "HIGH_COUNT_USED BIGINT not null,"
  "LOW_NUMBER_OF_BYTES_USED BIGINT not null,"
  "CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,"
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.memory_summary_by_account_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_ALLOC BIGINT unsigned not null,"
  "COUNT_FREE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,"
  "LOW_COUNT_USED BIGINT not null,"
  "CURRENT_COUNT_USED BIGINT not null,"
  "HIGH_COUNT_USED BIGINT not null,"
  "LOW_NUMBER_OF_BYTES_USED BIGINT not null,"
  "CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,"
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_BY_HOST_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.memory_summary_by_host_by_event_name("
  "HOST CHAR(60) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_ALLOC BIGINT unsigned not null,"
  "COUNT_FREE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,"
  "LOW_COUNT_USED BIGINT not null,"
  "CURRENT_COUNT_USED BIGINT not null,"
  "HIGH_COUNT_USED BIGINT not null,"
  "LOW_NUMBER_OF_BYTES_USED BIGINT not null,"
  "CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,"
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.memory_summary_by_user_by_event_name("
  "USER CHAR(32) collate utf8_bin default null,"
  "EVENT_NAME VARCHAR(128) not null,"
  "COUNT_ALLOC BIGINT unsigned not null,"
  "COUNT_FREE BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_ALLOC BIGINT unsigned not null,"
  "SUM_NUMBER_OF_BYTES_FREE BIGINT unsigned not null,"
  "LOW_COUNT_USED BIGINT not null,"
  "CURRENT_COUNT_USED BIGINT not null,"
  "HIGH_COUNT_USED BIGINT not null,"
  "LOW_NUMBER_OF_BYTES_USED BIGINT not null,"
  "CURRENT_NUMBER_OF_BYTES_USED BIGINT not null,"
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_DIGEST
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_digest("
  "SCHEMA_NAME VARCHAR(64),"
  "DIGEST VARCHAR(32),"
  "DIGEST_TEXT LONGTEXT,"
  "COUNT_STAR BIGINT unsigned not null,"
  "SUM_TIMER_WAIT BIGINT unsigned not null,"
  "MIN_TIMER_WAIT BIGINT unsigned not null,"
  "AVG_TIMER_WAIT BIGINT unsigned not null,"
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "SUM_LOCK_TIME BIGINT unsigned not null,"
  "SUM_ERRORS BIGINT unsigned not null,"
  "SUM_WARNINGS BIGINT unsigned not null,"
  "SUM_ROWS_AFFECTED BIGINT unsigned not null,"
  "SUM_ROWS_SENT BIGINT unsigned not null,"
  "SUM_ROWS_EXAMINED BIGINT unsigned not null,"
  "SUM_CREATED_TMP_DISK_TABLES BIGINT unsigned not null,"
  "SUM_CREATED_TMP_TABLES BIGINT unsigned not null,"
  "SUM_SELECT_FULL_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_FULL_RANGE_JOIN BIGINT unsigned not null,"
  "SUM_SELECT_RANGE BIGINT unsigned not null,"
  "SUM_SELECT_RANGE_CHECK BIGINT unsigned not null,"
  "SUM_SELECT_SCAN BIGINT unsigned not null,"
  "SUM_SORT_MERGE_PASSES BIGINT unsigned not null,"
  "SUM_SORT_RANGE BIGINT unsigned not null,"
  "SUM_SORT_ROWS BIGINT unsigned not null,"
  "SUM_SORT_SCAN BIGINT unsigned not null,"
  "SUM_NO_INDEX_USED BIGINT unsigned not null,"
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) NOT NULL default 0,"
  "LAST_SEEN TIMESTAMP(0) NOT NULL default 0"
  ")ENGINE=PERFORMANCE_SCHEMA;";


SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_PROGRAM
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_program("
  "OBJECT_TYPE enum('EVENT', 'FUNCTION', 'PROCEDURE', 'TABLE', 'TRIGGER'),"
  "OBJECT_SCHEMA varchar(64) NOT NULL,"
  "OBJECT_NAME varchar(64) NOT NULL,"
  "COUNT_STAR bigint(20) unsigned NOT NULL,"
  "SUM_TIMER_WAIT bigint(20) unsigned NOT NULL,"
  "MIN_TIMER_WAIT bigint(20) unsigned NOT NULL,"
  "AVG_TIMER_WAIT bigint(20) unsigned NOT NULL,"
  "MAX_TIMER_WAIT bigint(20) unsigned NOT NULL,"
  "COUNT_STATEMENTS bigint(20) unsigned NOT NULL,"
  "SUM_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,"
  "MIN_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,"
  "AVG_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,"
  "MAX_STATEMENTS_WAIT bigint(20) unsigned NOT NULL,"
  "SUM_LOCK_TIME bigint(20) unsigned NOT NULL,"
  "SUM_ERRORS bigint(20) unsigned NOT NULL,"
  "SUM_WARNINGS bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_AFFECTED bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_SENT bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_EXAMINED bigint(20) unsigned NOT NULL,"
  "SUM_CREATED_TMP_DISK_TABLES bigint(20) unsigned NOT NULL,"
  "SUM_CREATED_TMP_TABLES bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_FULL_JOIN bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_FULL_RANGE_JOIN bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_RANGE bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_RANGE_CHECK bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_SCAN bigint(20) unsigned NOT NULL,"
  "SUM_SORT_MERGE_PASSES bigint(20) unsigned NOT NULL,"
  "SUM_SORT_RANGE bigint(20) unsigned NOT NULL,"
  "SUM_SORT_ROWS bigint(20) unsigned NOT NULL,"
  "SUM_SORT_SCAN bigint(20) unsigned NOT NULL,"
  "SUM_NO_INDEX_USED bigint(20) unsigned NOT NULL,"
  "SUM_NO_GOOD_INDEX_USED bigint(20) unsigned NOT NULL"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE PREPARED_STATEMENT_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.prepared_statements_instances("
  "OBJECT_INSTANCE_BEGIN bigint(20) unsigned NOT NULL,"
  "STATEMENT_ID bigint(20) unsigned NOT NULL,"
  "STATEMENT_NAME varchar(64) default NULL,"
  "SQL_TEXT longtext NOT NULL,"
  "OWNER_THREAD_ID bigint(20) unsigned NOT NULL,"
  "OWNER_EVENT_ID bigint(20) unsigned NOT NULL,"
  "OWNER_OBJECT_TYPE enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER') DEFAULT NULL,"
  "OWNER_OBJECT_SCHEMA varchar(64) DEFAULT NULL,"
  "OWNER_OBJECT_NAME varchar(64) DEFAULT NULL,"
  "TIMER_PREPARE bigint(20) unsigned NOT NULL,"
  "COUNT_REPREPARE bigint(20) unsigned NOT NULL,"
  "COUNT_EXECUTE bigint(20) unsigned NOT NULL,"
  "SUM_TIMER_EXECUTE bigint(20) unsigned NOT NULL,"
  "MIN_TIMER_EXECUTE bigint(20) unsigned NOT NULL,"
  "AVG_TIMER_EXECUTE bigint(20) unsigned NOT NULL,"
  "MAX_TIMER_EXECUTE bigint(20) unsigned NOT NULL,"
  "SUM_LOCK_TIME bigint(20) unsigned NOT NULL,"
  "SUM_ERRORS bigint(20) unsigned NOT NULL,"
  "SUM_WARNINGS bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_AFFECTED bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_SENT bigint(20) unsigned NOT NULL,"
  "SUM_ROWS_EXAMINED bigint(20) unsigned NOT NULL,"
  "SUM_CREATED_TMP_DISK_TABLES bigint(20) unsigned NOT NULL,"
  "SUM_CREATED_TMP_TABLES bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_FULL_JOIN bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_FULL_RANGE_JOIN bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_RANGE bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_RANGE_CHECK bigint(20) unsigned NOT NULL,"
  "SUM_SELECT_SCAN bigint(20) unsigned NOT NULL,"
  "SUM_SORT_MERGE_PASSES bigint(20) unsigned NOT NULL,"
  "SUM_SORT_RANGE bigint(20) unsigned NOT NULL,"
  "SUM_SORT_ROWS bigint(20) unsigned NOT NULL,"
  "SUM_SORT_SCAN bigint(20) unsigned NOT NULL,"
  "SUM_NO_INDEX_USED bigint(20) unsigned NOT NULL,"
  "SUM_NO_GOOD_INDEX_USED bigint(20) unsigned NOT NULL"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE replication_connection_configuration
--

SET @cmd="CREATE TABLE performance_schema.replication_connection_configuration("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "HOST CHAR(60) collate utf8_bin not null,"
  "PORT INTEGER not null,"
  "USER CHAR(32) collate utf8_bin not null,"
  "NETWORK_INTERFACE CHAR(60) collate utf8_bin not null,"
  "AUTO_POSITION ENUM('1','0') not null,"
  "SSL_ALLOWED ENUM('YES','NO','IGNORED') not null,"
  "SSL_CA_FILE VARCHAR(512) not null,"
  "SSL_CA_PATH VARCHAR(512) not null,"
  "SSL_CERTIFICATE VARCHAR(512) not null,"
  "SSL_CIPHER VARCHAR(512) not null,"
  "SSL_KEY VARCHAR(512) not null,"
  "SSL_VERIFY_SERVER_CERTIFICATE ENUM('YES','NO') not null,"
  "SSL_CRL_FILE VARCHAR(255) not null,"
  "SSL_CRL_PATH VARCHAR(255) not null,"
  "CONNECTION_RETRY_INTERVAL INTEGER not null,"
  "CONNECTION_RETRY_COUNT BIGINT unsigned not null,"
  "HEARTBEAT_INTERVAL DOUBLE(10,3) unsigned not null COMMENT 'Number of seconds after which a heartbeat will be sent .',"
  "TLS_VERSION VARCHAR(255) not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- TABLE replication_group_member_stats
--

SET @cmd="CREATE TABLE performance_schema.replication_group_member_stats("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "VIEW_ID CHAR(60) collate utf8_bin not null,"
  "MEMBER_ID CHAR(36) collate utf8_bin not null,"
  "COUNT_TRANSACTIONS_IN_QUEUE BIGINT unsigned not null,"
  "COUNT_TRANSACTIONS_CHECKED BIGINT unsigned not null,"
  "COUNT_CONFLICTS_DETECTED BIGINT unsigned not null,"
  "COUNT_TRANSACTIONS_ROWS_VALIDATING BIGINT unsigned not null,"
  "TRANSACTIONS_COMMITTED_ALL_MEMBERS LONGTEXT not null,"
  "LAST_CONFLICT_FREE_TRANSACTION TEXT not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- TABLE replication_group_members
--

SET @cmd="CREATE TABLE performance_schema.replication_group_members("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "MEMBER_ID CHAR(36) collate utf8_bin not null,"
  "MEMBER_HOST CHAR(60) collate utf8_bin not null,"
  "MEMBER_PORT INTEGER,"
  "MEMBER_STATE CHAR(64) collate utf8_bin not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

 SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
 PREPARE stmt FROM @str;
 EXECUTE stmt;
 DROP PREPARE stmt;

--
-- TABLE replication_connection_status
--

SET @cmd="CREATE TABLE performance_schema.replication_connection_status("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "GROUP_NAME CHAR(36) collate utf8_bin not null,"
  "SOURCE_UUID CHAR(36) collate utf8_bin not null,"
  "THREAD_ID BIGINT unsigned,"
  "SERVICE_STATE ENUM('ON','OFF','CONNECTING') not null,"
  "COUNT_RECEIVED_HEARTBEATS bigint unsigned NOT NULL DEFAULT 0,"
  "LAST_HEARTBEAT_TIMESTAMP TIMESTAMP(0) not null COMMENT 'Shows when the most recent heartbeat signal was received.',"
  "RECEIVED_TRANSACTION_SET LONGTEXT not null,"
  "LAST_ERROR_NUMBER INTEGER not null,"
  "LAST_ERROR_MESSAGE VARCHAR(1024) not null,"
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE replication_applier_configuration
--

SET @cmd="CREATE TABLE performance_schema.replication_applier_configuration("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "DESIRED_DELAY INTEGER not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE replication_applier_status
--

SET @cmd="CREATE TABLE performance_schema.replication_applier_status("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "SERVICE_STATE ENUM('ON','OFF') not null,"
  "REMAINING_DELAY INTEGER unsigned,"
  "COUNT_TRANSACTIONS_RETRIES BIGINT unsigned not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE replication_applier_status_by_coordinator
--

SET @cmd="CREATE TABLE performance_schema.replication_applier_status_by_coordinator("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "THREAD_ID BIGINT UNSIGNED,"
  "SERVICE_STATE ENUM('ON','OFF') not null,"
  "LAST_ERROR_NUMBER INTEGER not null,"
  "LAST_ERROR_MESSAGE VARCHAR(1024) not null,"
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE replication_applier_status_by_worker
--

SET @cmd="CREATE TABLE performance_schema.replication_applier_status_by_worker("
  "CHANNEL_NAME CHAR(64) collate utf8_general_ci not null,"
  "WORKER_ID BIGINT UNSIGNED not null,"
  "THREAD_ID BIGINT UNSIGNED,"
  "SERVICE_STATE ENUM('ON','OFF') not null,"
  "LAST_SEEN_TRANSACTION CHAR(57) not null,"
  "LAST_ERROR_NUMBER INTEGER not null,"
  "LAST_ERROR_MESSAGE VARCHAR(1024) not null,"
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null"
  ") ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SESSION_CONNECT_ATTRS
--

SET @cmd="CREATE TABLE performance_schema.session_connect_attrs("
  "PROCESSLIST_ID INT NOT NULL,"
  "ATTR_NAME VARCHAR(32) NOT NULL,"
  "ATTR_VALUE VARCHAR(1024),"
  "ORDINAL_POSITION INT"
  ")ENGINE=PERFORMANCE_SCHEMA CHARACTER SET utf8 COLLATE utf8_bin;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SESSION_ACCOUNT_CONNECT_ATTRS
--

SET @cmd="CREATE TABLE performance_schema.session_account_connect_attrs "
         " LIKE performance_schema.session_connect_attrs;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE TABLE_HANDLES
--

SET @cmd="CREATE TABLE performance_schema.table_handles("
  "OBJECT_TYPE VARCHAR(64) not null,"
  "OBJECT_SCHEMA VARCHAR(64) not null,"
  "OBJECT_NAME VARCHAR(64) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "OWNER_THREAD_ID BIGINT unsigned,"
  "OWNER_EVENT_ID BIGINT unsigned,"
  "INTERNAL_LOCK VARCHAR(64),"
  "EXTERNAL_LOCK VARCHAR(64)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE METADATA_LOCKS
--

SET @cmd="CREATE TABLE performance_schema.metadata_locks("
  "OBJECT_TYPE VARCHAR(64) not null,"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "LOCK_TYPE VARCHAR(32) not null,"
  "LOCK_DURATION VARCHAR(32) not null,"
  "LOCK_STATUS VARCHAR(32) not null,"
  "SOURCE VARCHAR(64),"
  "OWNER_THREAD_ID BIGINT unsigned,"
  "OWNER_EVENT_ID BIGINT unsigned"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE USER_VARIABLES_BY_THREAD
--

SET @cmd="CREATE TABLE performance_schema.user_variables_by_thread("
  "THREAD_ID BIGINT unsigned not null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE LONGBLOB"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE VARIABLES_BY_THREAD
--

SET @cmd="CREATE TABLE performance_schema.variables_by_thread("
  "THREAD_ID BIGINT unsigned not null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE GLOBAL_VARIABLES
--

SET @cmd="CREATE TABLE performance_schema.global_variables("
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SESSION_VARIABLES
--

SET @cmd="CREATE TABLE performance_schema.session_variables("
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE STATUS_BY_THREAD
--

SET @cmd="CREATE TABLE performance_schema.status_by_thread("
  "THREAD_ID BIGINT unsigned not null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE STATUS_BY_USER
--

SET @cmd="CREATE TABLE performance_schema.status_by_user("
  "USER CHAR(32) collate utf8_bin default null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE STATUS_BY_HOST
--

SET @cmd="CREATE TABLE performance_schema.status_by_host("
  "HOST CHAR(60) collate utf8_bin default null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE STATUS_BY_ACCOUNT
--

SET @cmd="CREATE TABLE performance_schema.status_by_account("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE GLOBAL_STATUS
--

SET @cmd="CREATE TABLE performance_schema.global_status("
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE SESSION_STATUS
--

SET @cmd="CREATE TABLE performance_schema.session_status("
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024)"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

CREATE TABLE IF NOT EXISTS proxies_priv (Host char(60) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Proxied_host char(60) binary DEFAULT '' NOT NULL, Proxied_user char(32) binary DEFAULT '' NOT NULL, With_grant BOOL DEFAULT 0 NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY Host (Host,User,Proxied_host,Proxied_user), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='User proxy privileges';

-- Remember for later if proxies_priv table already existed
set @had_proxies_priv_table= @@warning_count != 0;

#
# SQL commands for creating the tables in MySQL Server which
# are used by the NDBINFO storage engine to access system
# information and statistics from MySQL Cluster
#
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

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Drop any old views in ndbinfo
SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`counters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`memoryusage`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`threadblocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cluster_transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`server_transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`cluster_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`server_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`arbitrator_validity_detail`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`arbitrator_validity_summary`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`memory_per_fragment`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`operations_per_fragment`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP VIEW IF EXISTS `ndbinfo`.`config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Drop any old lookup tables in ndbinfo
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$tables
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$tables`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$tables` (`table_id` INT UNSIGNED,`table_name` VARCHAR(512),`comment` VARCHAR(512)) COMMENT="metadata for tables available through ndbinfo" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$columns
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$columns`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$columns` (`table_id` INT UNSIGNED,`column_id` INT UNSIGNED,`column_name` VARCHAR(512),`column_type` INT UNSIGNED,`comment` VARCHAR(512)) COMMENT="metadata for columns available through ndbinfo " ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$test
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$test`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$test` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`counter` INT UNSIGNED,`counter2` BIGINT UNSIGNED) COMMENT="for testing" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$pools
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$pools`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$pools` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`pool_name` VARCHAR(512),`used` BIGINT UNSIGNED COMMENT "currently in use",`total` BIGINT UNSIGNED COMMENT "total allocated",`high` BIGINT UNSIGNED COMMENT "in use high water mark",`entry_size` BIGINT UNSIGNED COMMENT "size in bytes of each object",`config_param1` INT UNSIGNED COMMENT "config param 1 affecting pool",`config_param2` INT UNSIGNED COMMENT "config param 2 affecting pool",`config_param3` INT UNSIGNED COMMENT "config param 3 affecting pool",`config_param4` INT UNSIGNED COMMENT "config param 4 affecting pool") COMMENT="pool usage" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$transporters
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$transporters` (`node_id` INT UNSIGNED COMMENT "Node id reporting",`remote_node_id` INT UNSIGNED COMMENT "Node id at other end of link",`connection_status` INT UNSIGNED COMMENT "State of inter-node link",`remote_address` VARCHAR(512) COMMENT "Address of remote node",`bytes_sent` BIGINT UNSIGNED COMMENT "Bytes sent to remote node",`bytes_received` BIGINT UNSIGNED COMMENT "Bytes received from remote node",`connect_count` INT UNSIGNED COMMENT "Number of times connected",`overloaded` INT UNSIGNED COMMENT "Is link reporting overload",`overload_count` INT UNSIGNED COMMENT "Number of overload onsets since connect",`slowdown` INT UNSIGNED COMMENT "Is link requesting slowdown",`slowdown_count` INT UNSIGNED COMMENT "Number of slowdown onsets since connect") COMMENT="transporter status" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$logspaces
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$logspaces` (`node_id` INT UNSIGNED,`log_type` INT UNSIGNED COMMENT "0 = REDO, 1 = DD-UNDO",`log_id` INT UNSIGNED,`log_part` INT UNSIGNED,`total` BIGINT UNSIGNED COMMENT "total allocated",`used` BIGINT UNSIGNED COMMENT "currently in use",`high` BIGINT UNSIGNED COMMENT "in use high water mark") COMMENT="logspace usage" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$logbuffers
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$logbuffers` (`node_id` INT UNSIGNED,`log_type` INT UNSIGNED COMMENT "0 = REDO, 1 = DD-UNDO",`log_id` INT UNSIGNED,`log_part` INT UNSIGNED,`total` BIGINT UNSIGNED COMMENT "total allocated",`used` BIGINT UNSIGNED COMMENT "currently in use",`high` BIGINT UNSIGNED COMMENT "in use high water mark") COMMENT="logbuffer usage" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$resources
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$resources` (`node_id` INT UNSIGNED,`resource_id` INT UNSIGNED,`reserved` INT UNSIGNED COMMENT "reserved for this resource",`used` INT UNSIGNED COMMENT "currently in use",`max` INT UNSIGNED COMMENT "max available",`high` INT UNSIGNED COMMENT "in use high water mark") COMMENT="resources usage (a.k.a superpool)" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$counters
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$counters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$counters` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`counter_id` INT UNSIGNED,`val` BIGINT UNSIGNED COMMENT "monotonically increasing since process start") COMMENT="monotonic counters" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$nodes
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$nodes` (`node_id` INT UNSIGNED,`uptime` BIGINT UNSIGNED COMMENT "time in seconds that node has been running",`status` INT UNSIGNED COMMENT "starting/started/stopped etc.",`start_phase` INT UNSIGNED COMMENT "start phase if node is starting",`config_generation` INT UNSIGNED COMMENT "configuration generation number") COMMENT="node status" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$diskpagebuffer
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$diskpagebuffer` (`node_id` INT UNSIGNED,`block_instance` INT UNSIGNED,`pages_written` BIGINT UNSIGNED COMMENT "Pages written to disk",`pages_written_lcp` BIGINT UNSIGNED COMMENT "Pages written by local checkpoint",`pages_read` BIGINT UNSIGNED COMMENT "Pages read from disk",`log_waits` BIGINT UNSIGNED COMMENT "Page writes waiting for log to be written to disk",`page_requests_direct_return` BIGINT UNSIGNED COMMENT "Page in buffer and no requests waiting for it",`page_requests_wait_queue` BIGINT UNSIGNED COMMENT "Page in buffer, but some requests are already waiting for it",`page_requests_wait_io` BIGINT UNSIGNED COMMENT "Page not in buffer, waiting to be read from disk") COMMENT="disk page buffer info" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threadblocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threadblocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threadblocks` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "thread number",`block_number` INT UNSIGNED COMMENT "block number",`block_instance` INT UNSIGNED COMMENT "block instance") COMMENT="which blocks are run in which threads" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threadstat
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threadstat` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "thread number",`thr_nm` VARCHAR(512) COMMENT "thread name",`c_loop` BIGINT UNSIGNED COMMENT "No of loops in main loop",`c_exec` BIGINT UNSIGNED COMMENT "No of signals executed",`c_wait` BIGINT UNSIGNED COMMENT "No of times waited for more input",`c_l_sent_prioa` BIGINT UNSIGNED COMMENT "No of prio A signals sent to own node",`c_l_sent_priob` BIGINT UNSIGNED COMMENT "No of prio B signals sent to own node",`c_r_sent_prioa` BIGINT UNSIGNED COMMENT "No of prio A signals sent to remote node",`c_r_sent_priob` BIGINT UNSIGNED COMMENT "No of prio B signals sent to remote node",`os_tid` BIGINT UNSIGNED COMMENT "OS thread id",`os_now` BIGINT UNSIGNED COMMENT "OS gettimeofday (millis)",`os_ru_utime` BIGINT UNSIGNED COMMENT "OS user CPU time (micros)",`os_ru_stime` BIGINT UNSIGNED COMMENT "OS system CPU time (micros)",`os_ru_minflt` BIGINT UNSIGNED COMMENT "OS page reclaims (soft page faults",`os_ru_majflt` BIGINT UNSIGNED COMMENT "OS page faults (hard page faults)",`os_ru_nvcsw` BIGINT UNSIGNED COMMENT "OS voluntary context switches",`os_ru_nivcsw` BIGINT UNSIGNED COMMENT "OS involuntary context switches") COMMENT="Statistics on execution threads" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$disk_write_speed_base
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_base` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`millis_ago` BIGINT UNSIGNED COMMENT "Milliseconds ago since this period finished",`millis_passed` BIGINT UNSIGNED COMMENT "Milliseconds passed in the period reported", `backup_lcp_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written by backup and LCP in the period",`redo_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written to REDO log in the period",`target_disk_write_speed` BIGINT UNSIGNED COMMENT "Target disk write speed in bytes per second at the measurement point") COMMENT="Actual speed of disk writes per LDM thread, base data" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$disk_write_speed_aggregate
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_aggregate` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`backup_lcp_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP last second",`redo_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log last second",`backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 10 seconds",`redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 10 seconds",`std_dev_backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 10 seconds",`std_dev_redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 10 seconds",`backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 60 seconds",`redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 60 seconds",`std_dev_backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 60 seconds",`std_dev_redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 60 seconds",`slowdowns_due_to_io_lag` BIGINT UNSIGNED COMMENT "Number of seconds that we slowed down disk writes due to REDO log IO lagging",`slowdowns_due_to_high_cpu` BIGINT UNSIGNED COMMENT "Number of seconds we slowed down disk writes due to high CPU usage of LDM thread",`disk_write_speed_set_to_min` BIGINT UNSIGNED COMMENT "Number of seconds we set disk write speed to a minimum",`current_target_disk_write_speed` BIGINT UNSIGNED COMMENT "Current target of disk write speed in bytes per second") COMMENT="Actual speed of disk writes per LDM thread, aggregate data" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$restart_info
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$restart_info` (`node_id` INT UNSIGNED COMMENT "node id",`node_restart_status` VARCHAR(256) COMMENT "Current state of node recovery",`node_restart_status_int` INT UNSIGNED COMMENT "Current state of node recovery as number",`secs_to_complete_node_failure` INT UNSIGNED COMMENT "Seconds to complete node failure handling",`secs_to_allocate_node_id` INT UNSIGNED COMMENT "Seconds from node failure completion to allocation of node id", `secs_to_include_in_heartbeat_protocol` INT UNSIGNED COMMENT "Seonds from allocation of node id to inclusion in HB protocol",`secs_until_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds from included in HB protocol until we wait for ndbcntr master",`secs_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds we waited for being accepted by NDBCNTR master to start",`secs_to_get_start_permitted` INT UNSIGNED COMMENT "Seconds from permit by master until all nodes accepted our start",`secs_to_wait_for_lcp_for_copy_meta_data` INT UNSIGNED COMMENT "Seconds waiting for LCP completion before copying meta data",`secs_to_copy_meta_data` INT UNSIGNED COMMENT "Seconds to copy meta data to starting node from master",`secs_to_include_node` INT UNSIGNED COMMENT "Seconds to wait for GCP and inclusion of all nodes into protocols",`secs_starting_node_to_request_local_recovery` INT UNSIGNED COMMENT "Seconds for starting node to request local recovery",`secs_for_local_recovery` INT UNSIGNED COMMENT "Seconds for local recovery in starting node",`secs_restore_fragments` INT UNSIGNED COMMENT "Seconds to restore fragments from LCP files",`secs_undo_disk_data` INT UNSIGNED COMMENT "Seconds to execute UNDO log on disk data part of records",`secs_exec_redo_log` INT UNSIGNED COMMENT "Seconds to execute REDO log on all restored fragments",`secs_index_rebuild` INT UNSIGNED COMMENT "Seconds to rebuild indexes on restored fragments",`secs_to_synchronize_starting_node` INT UNSIGNED COMMENT "Seconds to synchronize starting node from live nodes",`secs_wait_lcp_for_restart` INT UNSIGNED COMMENT "Seconds to wait for LCP start and completion before restart is completed",`secs_wait_subscription_handover` INT UNSIGNED COMMENT "Seconds waiting for handover of replication subscriptions",`total_restart_secs` INT UNSIGNED COMMENT "Total number of seconds from node failure until node is started again") COMMENT="Times of restart phases in seconds and current state" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$transactions
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$transactions` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "TC instance no",`objid` INT UNSIGNED COMMENT "Object id of transaction object",`apiref` INT UNSIGNED COMMENT "API reference",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`state` INT UNSIGNED COMMENT "Transaction state",`flags` INT UNSIGNED COMMENT "Transaction flags",`c_ops` INT UNSIGNED COMMENT "No of operations in transaction",`outstanding` INT UNSIGNED COMMENT "Currently outstanding request",`timer` INT UNSIGNED COMMENT "Timer (seconds)") COMMENT="transactions" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$operations
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$operations` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`objid` INT UNSIGNED COMMENT "Object id of operation object",`tcref` INT UNSIGNED COMMENT "TC reference",`apiref` INT UNSIGNED COMMENT "API reference",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`tableid` INT UNSIGNED COMMENT "Table id",`fragmentid` INT UNSIGNED COMMENT "Fragment id",`op` INT UNSIGNED COMMENT "Operation type",`state` INT UNSIGNED COMMENT "Operation state",`flags` INT UNSIGNED COMMENT "Operation flags") COMMENT="operations" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$membership
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$membership` (`node_id` INT UNSIGNED COMMENT "node id",`group_id` INT UNSIGNED COMMENT "node group id",`left_node` INT UNSIGNED COMMENT "Left node in heart beat chain",`right_node` INT UNSIGNED COMMENT "Right node in heart beat chain",`president` INT UNSIGNED COMMENT "President nodeid",`successor` INT UNSIGNED COMMENT "President successor",`dynamic_id` INT UNSIGNED COMMENT "President, Configured_heartbeat order",`arbitrator` INT UNSIGNED COMMENT "Arbitrator nodeid",`arb_ticket` VARCHAR(512) COMMENT "Arbitrator ticket",`arb_state` INT UNSIGNED COMMENT "Arbitrator state",`arb_connected` INT UNSIGNED COMMENT "Arbitrator connected",`conn_rank1_arbs` VARCHAR(512) COMMENT "Connected rank 1 arbitrators",`conn_rank2_arbs` VARCHAR(512) COMMENT "Connected rank 2 arbitrators") COMMENT="membership" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$frag_mem_use
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_mem_use`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_mem_use` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LDM instance number",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`rows` BIGINT UNSIGNED COMMENT "Number of rows in table",`fixed_elem_alloc_bytes` BIGINT UNSIGNED COMMENT "Number of bytes allocated for fixed-sized elements",`fixed_elem_free_bytes` BIGINT UNSIGNED COMMENT "Free bytes in fixed-size element pages",`fixed_elem_count` BIGINT UNSIGNED COMMENT "Number of fixed size elements in use",`fixed_elem_size_bytes` INT UNSIGNED COMMENT "Length of each fixed sized element in bytes",`var_elem_alloc_bytes` BIGINT UNSIGNED COMMENT "Number of bytes allocated for var-size elements",`var_elem_free_bytes` BIGINT UNSIGNED COMMENT "Free bytes in var-size element pages",`var_elem_count` BIGINT UNSIGNED COMMENT "Number of var size elements in use",`tuple_l2pmap_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in logical to physical page map for tuple store",`hash_index_l2pmap_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in logical to physical page map for the hash index",`hash_index_alloc_bytes` BIGINT UNSIGNED COMMENT "Bytes in linear hash map") COMMENT="Per fragment space information" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$frag_operations
# This table contains per-fragment usage statistics. Most values in this table
# reflects only user operations, meaning that they do not include the 
# activities of LCPs and COPY_FRAG scans.. The exceptions are the counters 
# that concern scan queueing and concurrency: tot_qd_frag_scans, 
# conc_frag_scans, conc_qd_plain_frag_scans and conc_qd_tup_frag_scans (though 
# LCPs currently does not use TUP scans).
# The values starting with 'tot_' are accumulated values, meaning that they 
# give the totalt count of some event or sum of some attribute attibute since 
# the node was last restarted.
# Also observe that this table does *not* reflect 'early aborts' in any way.
# By 'early aborts' we mean operations that are rejected by LQH before 
# beginning execution due to resource issues such as overloaded transporters 
# or lack of free space in internal structures.
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd='CREATE TABLE ndbinfo.ndb$frag_operations (
  node_id INT UNSIGNED COMMENT "node id",
  block_instance INT UNSIGNED COMMENT "LQH instance no",
  table_id INT UNSIGNED COMMENT "Table identity",
  fragment_num INT UNSIGNED COMMENT "Fragment number",
  tot_key_reads BIGINT UNSIGNED COMMENT "Total number of key reads received",
  tot_key_inserts BIGINT UNSIGNED COMMENT 
    "Total number of key inserts received",
  tot_key_updates BIGINT UNSIGNED COMMENT
    "Total number of key updates received",
# 'write' operations, meaning update if key exists, insert otherwise are used 
# by e.g. restore and sql REPLACE.
  tot_key_writes BIGINT UNSIGNED COMMENT 
    "Total number of key writes received",
  tot_key_deletes BIGINT UNSIGNED COMMENT
    "Total number of key deletes received",
# Number of key operations refused by the LDM due to either:
# - no matching key for update/delete.
# - key exists already for insert.
# - operation rejected by interpreted program.
# - internal errors such as checksum errors.
  tot_key_refs BIGINT UNSIGNED COMMENT
    "Total number of key operations refused by LDM",
  tot_key_attrinfo_bytes BIGINT UNSIGNED COMMENT
    "Total attrinfo bytes received for key operations",
  tot_key_keyinfo_bytes BIGINT UNSIGNED COMMENT
    "Total keyinfo bytes received for key operations",
  tot_key_prog_bytes BIGINT UNSIGNED COMMENT
    "Total bytes of filter programs for key operations",
  tot_key_inst_exec BIGINT UNSIGNED COMMENT
    "Total number of interpreter instructions executed for key operations",
  tot_key_bytes_returned BIGINT UNSIGNED COMMENT
    "Total number of bytes returned to client for key operations",
  tot_frag_scans BIGINT UNSIGNED COMMENT 
    "Total number of fragment scans received",
  tot_scan_rows_examined BIGINT UNSIGNED COMMENT
    "Total number of rows examined by scans",
  tot_scan_rows_returned BIGINT UNSIGNED COMMENT
    "Total number or rows returned to client",
  tot_scan_bytes_returned BIGINT UNSIGNED COMMENT
    "Total number of bytes returned to client by scans",
  tot_scan_prog_bytes BIGINT UNSIGNED COMMENT
    "Total bytes of scan filter programs",
  tot_scan_bound_bytes BIGINT UNSIGNED COMMENT "Total bytes of scan bounds",
  tot_scan_inst_exec BIGINT UNSIGNED COMMENT
    "Total number of interpreter instructions executed for scans",
  tot_qd_frag_scans BIGINT UNSIGNED COMMENT
    "Total number of fragment scans queued before exec",
  conc_frag_scans INT UNSIGNED COMMENT "Number of frag scans currently running",
  conc_qd_plain_frag_scans INT UNSIGNED COMMENT
    "Number of tux frag scans currently queued",
  conc_qd_tup_frag_scans INT UNSIGNED COMMENT
    "Number of tup frag scans currently queued",
  conc_qd_acc_frag_scans INT UNSIGNED COMMENT
    "Number of acc frag scans currently queued",
  tot_commits BIGINT UNSIGNED COMMENT "Total number of committed row changes")
COMMENT="Per fragment operational information" ENGINE=NDBINFO;';

SET @str=IF(@have_ndbinfo <> 0, @cmd, 'SET @dummy = 0;');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$blocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$blocks` (block_number INT UNSIGNED, block_name VARCHAR(512)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dict_obj_info
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dict_obj_info` (`type` INT UNSIGNED COMMENT "Type of dict object",`id` INT UNSIGNED COMMENT "Object identity",`version` INT UNSIGNED COMMENT "Object version",`state` INT UNSIGNED COMMENT "Object state",`parent_obj_type` INT UNSIGNED COMMENT "Parent object type",`parent_obj_id` INT UNSIGNED COMMENT "Parent object id",`fq_name` VARCHAR(512) COMMENT "Fully qualified object name") COMMENT="Dictionary object info" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dict_obj_types
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dict_obj_types` (type_id INT UNSIGNED, type_name VARCHAR(512)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$config_params
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_params` (param_number INT UNSIGNED, param_name VARCHAR(512)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dbtc_apiconnect_state
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dbtc_apiconnect_state` (state_int_value INT UNSIGNED, state_name VARCHAR(256), state_friendly_name VARCHAR(256), state_description VARCHAR(256)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$dblqh_tcconnect_state
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dblqh_tcconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dblqh_tcconnect_state` (state_int_value INT UNSIGNED, state_name VARCHAR(256), state_friendly_name VARCHAR(256), state_description VARCHAR(256)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.transporters
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`transporters` AS SELECT node_id, remote_node_id,  CASE connection_status  WHEN 0 THEN "CONNECTED"  WHEN 1 THEN "CONNECTING"  WHEN 2 THEN "DISCONNECTED"  WHEN 3 THEN "DISCONNECTING"  ELSE NULL  END AS status,  remote_address, bytes_sent, bytes_received,  connect_count,  overloaded, overload_count, slowdown, slowdown_count FROM `ndbinfo`.`ndb$transporters`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.logspaces
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`logspaces` AS SELECT node_id,  CASE log_type  WHEN 0 THEN "REDO"  WHEN 1 THEN "DD-UNDO"  ELSE NULL  END AS log_type, log_id, log_part, total, used FROM `ndbinfo`.`ndb$logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.logbuffers
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`logbuffers` AS SELECT node_id,  CASE log_type  WHEN 0 THEN "REDO"  WHEN 1 THEN "DD-UNDO"  ELSE "<unknown>"  END AS log_type, log_id, log_part, total, used FROM `ndbinfo`.`ndb$logbuffers`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.resources
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`resources` AS SELECT node_id,  CASE resource_id  WHEN 0 THEN "RESERVED"  WHEN 1 THEN "DISK_OPERATIONS"  WHEN 2 THEN "DISK_RECORDS"  WHEN 3 THEN "DATA_MEMORY"  WHEN 4 THEN "JOBBUFFER"  WHEN 5 THEN "FILE_BUFFERS"  WHEN 6 THEN "TRANSPORTER_BUFFERS"  WHEN 7 THEN "DISK_PAGE_BUFFER"  WHEN 8 THEN "QUERY_MEMORY"  WHEN 9 THEN "SCHEMA_TRANS_MEMORY"  ELSE "<unknown>"  END AS resource_name, reserved, used, max FROM `ndbinfo`.`ndb$resources`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.counters
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`counters` AS SELECT node_id, b.block_name, block_instance, counter_id, CASE counter_id  WHEN 1 THEN "ATTRINFO"  WHEN 2 THEN "TRANSACTIONS"  WHEN 3 THEN "COMMITS"  WHEN 4 THEN "READS"  WHEN 5 THEN "SIMPLE_READS"  WHEN 6 THEN "WRITES"  WHEN 7 THEN "ABORTS"  WHEN 8 THEN "TABLE_SCANS"  WHEN 9 THEN "RANGE_SCANS"  WHEN 10 THEN "OPERATIONS"  WHEN 11 THEN "READS_RECEIVED"  WHEN 12 THEN "LOCAL_READS_SENT"  WHEN 13 THEN "REMOTE_READS_SENT"  WHEN 14 THEN "READS_NOT_FOUND"  WHEN 15 THEN "TABLE_SCANS_RECEIVED"  WHEN 16 THEN "LOCAL_TABLE_SCANS_SENT"  WHEN 17 THEN "RANGE_SCANS_RECEIVED"  WHEN 18 THEN "LOCAL_RANGE_SCANS_SENT"  WHEN 19 THEN "REMOTE_RANGE_SCANS_SENT"  WHEN 20 THEN "SCAN_BATCHES_RETURNED"  WHEN 21 THEN "SCAN_ROWS_RETURNED"  WHEN 22 THEN "PRUNED_RANGE_SCANS_RECEIVED"  WHEN 23 THEN "CONST_PRUNED_RANGE_SCANS_RECEIVED"  WHEN 24 THEN "LOCAL_READS"  WHEN 25 THEN "LOCAL_WRITES"  WHEN 26 THEN "LQHKEY_OVERLOAD"  WHEN 27 THEN "LQHKEY_OVERLOAD_TC"  WHEN 28 THEN "LQHKEY_OVERLOAD_READER"  WHEN 29 THEN "LQHKEY_OVERLOAD_NODE_PEER"  WHEN 30 THEN "LQHKEY_OVERLOAD_SUBSCRIBER"  WHEN 31 THEN "LQHSCAN_SLOWDOWNS"  ELSE "<unknown>"  END AS counter_name, val FROM `ndbinfo`.`ndb$counters` c LEFT JOIN `ndbinfo`.`ndb$blocks` b ON c.block_number = b.block_number','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.nodes
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`nodes` AS SELECT node_id, uptime, CASE status  WHEN 0 THEN "NOTHING"  WHEN 1 THEN "CMVMI"  WHEN 2 THEN "STARTING"  WHEN 3 THEN "STARTED"  WHEN 4 THEN "SINGLEUSER"  WHEN 5 THEN "STOPPING_1"  WHEN 6 THEN "STOPPING_2"  WHEN 7 THEN "STOPPING_3"  WHEN 8 THEN "STOPPING_4"  ELSE "<unknown>"  END AS status, start_phase, config_generation FROM `ndbinfo`.`ndb$nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.memoryusage
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`memoryusage` AS SELECT node_id,  pool_name AS memory_type,  SUM(used*entry_size) AS used,  SUM(used) AS used_pages,  SUM(total*entry_size) AS total,  SUM(total) AS total_pages FROM `ndbinfo`.`ndb$pools` WHERE (block_number IN (248, 254) AND   (pool_name = "Index memory" OR pool_name = "Data memory")) OR pool_name = "Long message buffer" GROUP BY node_id, memory_type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.diskpagebuffer
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`diskpagebuffer` AS SELECT node_id, block_instance, pages_written, pages_written_lcp, pages_read, log_waits, page_requests_direct_return, page_requests_wait_queue, page_requests_wait_io FROM `ndbinfo`.`ndb$diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.diskpagebuffer
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`diskpagebuffer` AS SELECT node_id, block_instance, pages_written, pages_written_lcp, pages_read, log_waits, page_requests_direct_return, page_requests_wait_queue, page_requests_wait_io FROM `ndbinfo`.`ndb$diskpagebuffer`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.threadblocks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`threadblocks` AS SELECT t.node_id, t.thr_no, b.block_name, t.block_instance FROM `ndbinfo`.`ndb$threadblocks` t LEFT JOIN `ndbinfo`.`ndb$blocks` b ON t.block_number = b.block_number','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.threadstat
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`threadstat` AS SELECT * from `ndbinfo`.`ndb$threadstat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_base
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_base` AS SELECT * from `ndbinfo`.`ndb$disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_aggregate
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_aggregate` AS SELECT * from `ndbinfo`.`ndb$disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.disk_write_speed_aggregate_node
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`disk_write_speed_aggregate_node` AS SELECT node_id,SUM(backup_lcp_speed_last_sec) AS backup_lcp_speed_last_sec,SUM(redo_speed_last_sec) AS redo_speed_last_sec,SUM(backup_lcp_speed_last_10sec) AS backup_lcp_speed_last_10sec,SUM(redo_speed_last_10sec) AS redo_speed_last_10sec,SUM(backup_lcp_speed_last_60sec) AS backup_lcp_speed_last_60sec,SUM(redo_speed_last_60sec) AS redo_speed_last_60sec from `ndbinfo`.`ndb$disk_write_speed_aggregate` GROUP by node_id','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.restart_info
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`restart_info` AS SELECT * from `ndbinfo`.`ndb$restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cluster_transactions
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cluster_transactions` AS SELECT t.node_id, t.block_instance, t.transid0 + (t.transid1 << 32) as transid, s.state_friendly_name as state,  t.c_ops as count_operations,  t.outstanding as outstanding_operations,  t.timer as inactive_seconds,  (t.apiref & 65535) as client_node_id,  (t.apiref >> 16) as client_block_ref FROM `ndbinfo`.`ndb$transactions` t LEFT JOIN `ndbinfo`.`ndb$dbtc_apiconnect_state` s        ON s.state_int_value = t.state','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.server_transactions
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`server_transactions` AS SELECT map.mysql_connection_id, t.*FROM information_schema.ndb_transid_mysql_connection_map map JOIN `ndbinfo`.cluster_transactions t   ON (map.ndb_transid >> 32) = (t.transid >> 32)','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.cluster_operations
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`cluster_operations` AS SELECT o.node_id, o.block_instance, o.transid0 + (o.transid1 << 32) as transid, case o.op  when 1 then "READ" when 2 then "READ-SH" when 3 then "READ-EX" when 4 then "INSERT" when 5 then "UPDATE" when 6 then "DELETE" when 7 then "WRITE" when 8 then "UNLOCK" when 9 then "REFRESH" when 257 then "SCAN" when 258 then "SCAN-SH" when 259 then "SCAN-EX" ELSE "<unknown>" END as operation_type,  s.state_friendly_name as state,  o.tableid,  o.fragmentid,  (o.apiref & 65535) as client_node_id,  (o.apiref >> 16) as client_block_ref,  (o.tcref & 65535) as tc_node_id,  ((o.tcref >> 16) & 511) as tc_block_no,  ((o.tcref >> (16 + 9)) & 127) as tc_block_instance FROM `ndbinfo`.`ndb$operations` o LEFT JOIN `ndbinfo`.`ndb$dblqh_tcconnect_state` s        ON s.state_int_value = o.state','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.server_operations
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`server_operations` AS SELECT map.mysql_connection_id, o.* FROM `ndbinfo`.cluster_operations o JOIN information_schema.ndb_transid_mysql_connection_map map  ON (map.ndb_transid >> 32) = (o.transid >> 32)','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.membership
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`membership` AS SELECT node_id, group_id, left_node, right_node, president, successor, dynamic_id & 0xFFFF AS succession_order, dynamic_id >> 16 AS Conf_HB_order, arbitrator, arb_ticket, CASE arb_state  WHEN 0 THEN "ARBIT_NULL"  WHEN 1 THEN "ARBIT_INIT"  WHEN 2 THEN "ARBIT_FIND"  WHEN 3 THEN "ARBIT_PREP1"  WHEN 4 THEN "ARBIT_PREP2"  WHEN 5 THEN "ARBIT_START"  WHEN 6 THEN "ARBIT_RUN"  WHEN 7 THEN "ARBIT_CHOOSE"  WHEN 8 THEN "ARBIT_CRASH"  ELSE "UNKNOWN" END AS arb_state, CASE arb_connected  WHEN 1 THEN "Yes"  ELSE "No" END AS arb_connected, conn_rank1_arbs AS connected_rank1_arbs, conn_rank2_arbs AS connected_rank2_arbs FROM `ndbinfo`.`ndb$membership`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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


# ndbinfo.memory_per_fragment
# The test for name.type<=6 is there to elimiate matching non-table objects 
# (triggers, files etc.), since the 'id' of these may collide with table ids.
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`memory_per_fragment` AS SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, types.type_name AS type, table_id, node_id, block_instance, fragment_num, fixed_elem_alloc_bytes, fixed_elem_free_bytes, fixed_elem_size_bytes, fixed_elem_count, FLOOR(fixed_elem_free_bytes/fixed_elem_size_bytes) AS fixed_elem_free_count, var_elem_alloc_bytes, var_elem_free_bytes, var_elem_count, hash_index_alloc_bytes FROM ndbinfo.ndb$frag_mem_use AS space JOIN ndbinfo.ndb$dict_obj_info AS name ON name.id=space.table_id AND name.type<=6 JOIN ndbinfo.ndb$dict_obj_types AS types ON name.type=types.type_id LEFT JOIN ndbinfo.ndb$dict_obj_info AS parent_name ON name.parent_obj_id=parent_name.id AND name.parent_obj_type=parent_name.type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Finally turn off offline mode
SET @str=IF(@have_ndbinfo,'SET @@global.ndbinfo_offline=FALSE','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.operations_per_fragment
# This is the end-user view of ndb$frag_operations. It adds some 
# dictionary information such as the table name and type, and the name of the
# parent table, if there is any.
#
# The test for name.type<=6 is there to elimiate matching non-table objects 
# (triggers, files etc.), since the 'id' of these may collide with table ids.
SET @cmd='CREATE OR REPLACE DEFINER=`root`@`localhost` 
  SQL SECURITY INVOKER VIEW `ndbinfo`.`operations_per_fragment` AS 
  SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, 
    types.type_name AS type, table_id, node_id, block_instance, fragment_num, 
    tot_key_reads, tot_key_inserts, tot_key_updates, tot_key_writes, 
    tot_key_deletes, tot_key_refs, tot_key_attrinfo_bytes,
    tot_key_keyinfo_bytes, tot_key_prog_bytes, tot_key_inst_exec, 
    tot_key_bytes_returned, tot_frag_scans, tot_scan_rows_examined, 
    tot_scan_rows_returned, tot_scan_bytes_returned, tot_scan_prog_bytes, 
    tot_scan_bound_bytes, tot_scan_inst_exec, tot_qd_frag_scans, 
    conc_frag_scans,
    conc_qd_plain_frag_scans+conc_qd_tup_frag_scans+conc_qd_acc_frag_scans
    AS conc_qd_frag_scans,
    tot_commits 
    FROM ndbinfo.ndb$frag_operations AS ops 
    JOIN ndbinfo.ndb$dict_obj_info AS name 
      ON name.id=ops.table_id AND name.type<=6 
    JOIN ndbinfo.ndb$dict_obj_types AS types ON name.type=types.type_id
    LEFT JOIN ndbinfo.ndb$dict_obj_info AS parent_name 
      ON name.parent_obj_id=parent_name.id AND 
         name.parent_obj_type=parent_name.type';
SET @str=IF(@have_ndbinfo <> 0, @cmd, 'SET @dummy = 0;');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.blocks
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`blocks` AS SELECT block_number, block_name FROM `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.dict_obj_types
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`dict_obj_types` AS SELECT type_id, type_name FROM `ndbinfo`.`ndb$dict_obj_types`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.config_params
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`config_params` AS SELECT param_number, param_name FROM `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# Finally turn off offline mode
SET @str=IF(@have_ndbinfo,'SET @@global.ndbinfo_offline=FALSE','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;
