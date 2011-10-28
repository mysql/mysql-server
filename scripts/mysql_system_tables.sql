-- Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.
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
set storage_engine=myisam;

CREATE TABLE IF NOT EXISTS db (   Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,Db,User), KEY User (User) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='Database privileges';

-- Remember for later if db table already existed
set @had_db_table= @@warning_count != 0;

CREATE TABLE IF NOT EXISTS host (  Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,Db) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='Host privileges;  Merged with database privileges';


CREATE TABLE IF NOT EXISTS user (   Host char(60) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Password char(41) character set latin1 collate latin1_bin DEFAULT '' NOT NULL, Select_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Reload_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Shutdown_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Process_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, File_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, References_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_db_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Super_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Repl_slave_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Repl_client_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Show_view_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Alter_routine_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_user_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Event_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Trigger_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, Create_tablespace_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, ssl_type enum('','ANY','X509', 'SPECIFIED') COLLATE utf8_general_ci DEFAULT '' NOT NULL, ssl_cipher BLOB NOT NULL, x509_issuer BLOB NOT NULL, x509_subject BLOB NOT NULL, max_questions int(11) unsigned DEFAULT 0  NOT NULL, max_updates int(11) unsigned DEFAULT 0  NOT NULL, max_connections int(11) unsigned DEFAULT 0  NOT NULL, max_user_connections int(11) unsigned DEFAULT 0  NOT NULL, plugin char(64) DEFAULT '', authentication_string TEXT, PRIMARY KEY Host (Host,User) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='Users and global privileges';

-- Remember for later if user table already existed
set @had_user_table= @@warning_count != 0;


CREATE TABLE IF NOT EXISTS func (  name char(64) binary DEFAULT '' NOT NULL, ret tinyint(1) DEFAULT '0' NOT NULL, dl char(128) DEFAULT '' NOT NULL, type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL, PRIMARY KEY (name) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='User defined functions';


CREATE TABLE IF NOT EXISTS plugin ( name varchar(64) DEFAULT '' NOT NULL, dl varchar(128) DEFAULT '' NOT NULL, PRIMARY KEY (name) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_general_ci comment='MySQL plugins';


CREATE TABLE IF NOT EXISTS servers ( Server_name char(64) NOT NULL DEFAULT '', Host char(64) NOT NULL DEFAULT '', Db char(64) NOT NULL DEFAULT '', Username char(64) NOT NULL DEFAULT '', Password char(64) NOT NULL DEFAULT '', Port INT(4) NOT NULL DEFAULT '0', Socket char(64) NOT NULL DEFAULT '', Wrapper char(64) NOT NULL DEFAULT '', Owner char(64) NOT NULL DEFAULT '', PRIMARY KEY (Server_name)) CHARACTER SET utf8 comment='MySQL Foreign Servers table';


CREATE TABLE IF NOT EXISTS tables_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(77) DEFAULT '' NOT NULL, Timestamp timestamp, Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Table privileges';

CREATE TABLE IF NOT EXISTS columns_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL, Timestamp timestamp, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Column privileges';


CREATE TABLE IF NOT EXISTS help_topic ( help_topic_id int unsigned not null, name char(64) not null, help_category_id smallint unsigned not null, description text not null, example  text not null, url char(128) not null, primary key (help_topic_id), unique index (name) ) engine=MyISAM CHARACTER SET utf8   comment='help topics';


CREATE TABLE IF NOT EXISTS help_category ( help_category_id smallint unsigned not null, name  char(64) not null, parent_category_id smallint unsigned null, url char(128) not null, primary key (help_category_id), unique index (name) ) engine=MyISAM CHARACTER SET utf8   comment='help categories';


CREATE TABLE IF NOT EXISTS help_relation ( help_topic_id int unsigned not null references help_topic, help_keyword_id  int unsigned not null references help_keyword, primary key (help_keyword_id, help_topic_id) ) engine=MyISAM CHARACTER SET utf8 comment='keyword-topic relation';


CREATE TABLE IF NOT EXISTS help_keyword (   help_keyword_id  int unsigned not null, name char(64) not null, primary key (help_keyword_id), unique index (name) ) engine=MyISAM CHARACTER SET utf8 comment='help keywords';


CREATE TABLE IF NOT EXISTS time_zone_name (   Name char(64) NOT NULL, Time_zone_id int unsigned NOT NULL, PRIMARY KEY Name (Name) ) engine=MyISAM CHARACTER SET utf8   comment='Time zone names';


CREATE TABLE IF NOT EXISTS time_zone (   Time_zone_id int unsigned NOT NULL auto_increment, Use_leap_seconds enum('Y','N') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY TzId (Time_zone_id) ) engine=MyISAM CHARACTER SET utf8   comment='Time zones';


CREATE TABLE IF NOT EXISTS time_zone_transition (   Time_zone_id int unsigned NOT NULL, Transition_time bigint signed NOT NULL, Transition_type_id int unsigned NOT NULL, PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time) ) engine=MyISAM CHARACTER SET utf8   comment='Time zone transitions';


CREATE TABLE IF NOT EXISTS time_zone_transition_type (   Time_zone_id int unsigned NOT NULL, Transition_type_id int unsigned NOT NULL, Offset int signed DEFAULT 0 NOT NULL, Is_DST tinyint unsigned DEFAULT 0 NOT NULL, Abbreviation char(8) DEFAULT '' NOT NULL, PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id) ) engine=MyISAM CHARACTER SET utf8   comment='Time zone transition types';


CREATE TABLE IF NOT EXISTS time_zone_leap_second (   Transition_time bigint signed NOT NULL, Correction int signed NOT NULL, PRIMARY KEY TranTime (Transition_time) ) engine=MyISAM CHARACTER SET utf8   comment='Leap seconds information for time zones';


CREATE TABLE IF NOT EXISTS proc (db char(64) collate utf8_bin DEFAULT '' NOT NULL, name char(64) DEFAULT '' NOT NULL, type enum('FUNCTION','PROCEDURE') NOT NULL, specific_name char(64) DEFAULT '' NOT NULL, language enum('SQL') DEFAULT 'SQL' NOT NULL, sql_data_access enum( 'CONTAINS_SQL', 'NO_SQL', 'READS_SQL_DATA', 'MODIFIES_SQL_DATA') DEFAULT 'CONTAINS_SQL' NOT NULL, is_deterministic enum('YES','NO') DEFAULT 'NO' NOT NULL, security_type enum('INVOKER','DEFINER') DEFAULT 'DEFINER' NOT NULL, param_list blob NOT NULL, returns longblob DEFAULT '' NOT NULL, body longblob NOT NULL, definer char(77) collate utf8_bin DEFAULT '' NOT NULL, created timestamp, modified timestamp, sql_mode set( 'REAL_AS_FLOAT', 'PIPES_AS_CONCAT', 'ANSI_QUOTES', 'IGNORE_SPACE', 'NOT_USED', 'ONLY_FULL_GROUP_BY', 'NO_UNSIGNED_SUBTRACTION', 'NO_DIR_IN_CREATE', 'POSTGRESQL', 'ORACLE', 'MSSQL', 'DB2', 'MAXDB', 'NO_KEY_OPTIONS', 'NO_TABLE_OPTIONS', 'NO_FIELD_OPTIONS', 'MYSQL323', 'MYSQL40', 'ANSI', 'NO_AUTO_VALUE_ON_ZERO', 'NO_BACKSLASH_ESCAPES', 'STRICT_TRANS_TABLES', 'STRICT_ALL_TABLES', 'NO_ZERO_IN_DATE', 'NO_ZERO_DATE', 'INVALID_DATES', 'ERROR_FOR_DIVISION_BY_ZERO', 'TRADITIONAL', 'NO_AUTO_CREATE_USER', 'HIGH_NOT_PRECEDENCE', 'NO_ENGINE_SUBSTITUTION', 'PAD_CHAR_TO_FULL_LENGTH') DEFAULT '' NOT NULL, comment text collate utf8_bin NOT NULL, character_set_client char(32) collate utf8_bin, collation_connection char(32) collate utf8_bin, db_collation char(32) collate utf8_bin, body_utf8 longblob, PRIMARY KEY (db,name,type)) engine=MyISAM character set utf8 comment='Stored Procedures';

CREATE TABLE IF NOT EXISTS procs_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Routine_name char(64) COLLATE utf8_general_ci DEFAULT '' NOT NULL, Routine_type enum('FUNCTION','PROCEDURE') NOT NULL, Grantor char(77) DEFAULT '' NOT NULL, Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Timestamp timestamp, PRIMARY KEY (Host,Db,User,Routine_name,Routine_type), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin   comment='Procedure privileges';

-- Create general_log if CSV is enabled.
SET @have_csv = (SELECT support FROM information_schema.engines WHERE engine = 'CSV');
SET @str = IF (@have_csv = 'YES', 'CREATE TABLE IF NOT EXISTS general_log (event_time TIMESTAMP NOT NULL, user_host MEDIUMTEXT NOT NULL, thread_id INTEGER NOT NULL, server_id INTEGER UNSIGNED NOT NULL, command_type VARCHAR(64) NOT NULL, argument MEDIUMTEXT NOT NULL) engine=CSV CHARACTER SET utf8 comment="General log"', 'SET @dummy = 0');

PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

-- Create slow_log if CSV is enabled.

SET @str = IF (@have_csv = 'YES', 'CREATE TABLE IF NOT EXISTS slow_log (start_time TIMESTAMP NOT NULL, user_host MEDIUMTEXT NOT NULL, query_time TIME NOT NULL, lock_time TIME NOT NULL, rows_sent INTEGER NOT NULL, rows_examined INTEGER NOT NULL, db VARCHAR(512) NOT NULL, last_insert_id INTEGER NOT NULL, insert_id INTEGER NOT NULL, server_id INTEGER UNSIGNED NOT NULL, sql_text MEDIUMTEXT NOT NULL, thread_id INTEGER NOT NULL) engine=CSV CHARACTER SET utf8 comment="Slow log"', 'SET @dummy = 0');

PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

CREATE TABLE IF NOT EXISTS event ( db char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', name char(64) CHARACTER SET utf8 NOT NULL default '', body longblob NOT NULL, definer char(77) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', execute_at DATETIME default NULL, interval_value int(11) default NULL, interval_field ENUM('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND','DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND','SECOND_MICROSECOND') default NULL, created TIMESTAMP NOT NULL, modified TIMESTAMP NOT NULL, last_executed DATETIME default NULL, starts DATETIME default NULL, ends DATETIME default NULL, status ENUM('ENABLED','DISABLED','SLAVESIDE_DISABLED') NOT NULL default 'ENABLED', on_completion ENUM('DROP','PRESERVE') NOT NULL default 'DROP', sql_mode  set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB','NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40','ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH') DEFAULT '' NOT NULL, comment char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '', originator INTEGER UNSIGNED NOT NULL, time_zone char(64) CHARACTER SET latin1 NOT NULL DEFAULT 'SYSTEM', character_set_client char(32) collate utf8_bin, collation_connection char(32) collate utf8_bin, db_collation char(32) collate utf8_bin, body_utf8 longblob, PRIMARY KEY (db, name) ) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT 'Events';


CREATE TABLE IF NOT EXISTS ndb_binlog_index (Position BIGINT UNSIGNED NOT NULL, File VARCHAR(255) NOT NULL, epoch BIGINT UNSIGNED NOT NULL, inserts BIGINT UNSIGNED NOT NULL, updates BIGINT UNSIGNED NOT NULL, deletes BIGINT UNSIGNED NOT NULL, schemaops BIGINT UNSIGNED NOT NULL, PRIMARY KEY(epoch)) ENGINE=MYISAM;

CREATE TABLE IF NOT EXISTS slave_relay_log_info (Master_id INTEGER UNSIGNED NOT NULL, Number_of_lines INTEGER UNSIGNED NOT NULL COMMENT 'Number of lines in the file or rows in the table. Used to version table definitions.', Relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the current relay log file.', Relay_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The relay log position of the last executed event.', Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the master binary log file from which the events in the relay log file were read.', Master_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The master log position of the last executed event.', Sql_delay INTEGER NOT NULL COMMENT 'The number of seconds that the slave must lag behind the master.', Number_of_workers INTEGER UNSIGNED NOT NULL, PRIMARY KEY(Master_id)) ENGINE=MYISAM DEFAULT CHARSET=utf8 COMMENT 'Relay Log Information';

CREATE TABLE IF NOT EXISTS slave_master_info (Master_id INTEGER UNSIGNED NOT NULL, Number_of_lines INTEGER UNSIGNED NOT NULL COMMENT 'Number of lines in the file.', Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL COMMENT 'The name of the master binary log currently being read from the master.', Master_log_pos BIGINT UNSIGNED NOT NULL COMMENT 'The master log position of the last read event.', Host TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The host name of the master.', User_name TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The user name used to connect to the master.', User_password TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The password used to connect to the master.', Port INTEGER UNSIGNED NOT NULL COMMENT 'The network port used to connect to the master.', Connect_retry INTEGER UNSIGNED NOT NULL COMMENT 'The period (in seconds) that the slave will wait before trying to reconnect to the master.', Enabled_ssl BOOLEAN NOT NULL COMMENT 'Indicates whether the server supports SSL connections.', Ssl_ca TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The file used for the Certificate Authority (CA) certificate.', Ssl_capath TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The path to the Certificate Authority (CA) certificates.', Ssl_cert TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the SSL certificate file.', Ssl_cipher TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the cipher in use for the SSL connection.', Ssl_key TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The name of the SSL key file.', Ssl_verify_server_cert BOOLEAN NOT NULL COMMENT 'Whether to verify the server certificate.', Heartbeat FLOAT NOT NULL COMMENT '', Bind TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'Displays which interface is employed when connecting to the MySQL server', Ignored_server_ids TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The number of server IDs to be ignored, followed by the actual server IDs', Uuid TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The master server uuid.', Retry_count BIGINT UNSIGNED NOT NULL COMMENT 'Number of reconnect attempts, to the master, before giving up.', Ssl_crl TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The file used for the Certificate Revocation List (CRL)', Ssl_crlpath TEXT CHARACTER SET utf8 COLLATE utf8_bin COMMENT 'The path used for Certificate Revocation List (CRL) files', PRIMARY KEY(Master_id)) ENGINE=MYISAM DEFAULT CHARSET=utf8 COMMENT 'Master Information';

CREATE TABLE IF NOT EXISTS slave_worker_info (Master_id INTEGER UNSIGNED NOT NULL, Worker_id INTEGER UNSIGNED NOT NULL, Relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, Relay_log_pos BIGINT UNSIGNED NOT NULL, Master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, Master_log_pos BIGINT UNSIGNED NOT NULL, Checkpoint_relay_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, Checkpoint_relay_log_pos BIGINT UNSIGNED NOT NULL, Checkpoint_master_log_name TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, Checkpoint_master_log_pos BIGINT UNSIGNED NOT NULL, Checkpoint_seqno INT UNSIGNED NOT NULL, Checkpoint_group_size INTEGER UNSIGNED NOT NULL, Checkpoint_group_bitmap BLOB NOT NULL, PRIMARY KEY(Master_id, Worker_id)) ENGINE=MYISAM DEFAULT CHARSET=utf8 COMMENT 'Worker Information';

SET @sql_mode_orig=@@SESSION.sql_mode;
SET SESSION sql_mode='NO_ENGINE_SUBSTITUTION';

CREATE TABLE IF NOT EXISTS innodb_table_stats (
   database_name        VARCHAR(64) NOT NULL,
   table_name        VARCHAR(64) NOT NULL,
   stats_timestamp         TIMESTAMP NOT NULL,
   n_rows            BIGINT UNSIGNED NOT NULL,
   clustered_index_size    BIGINT UNSIGNED NOT NULL,
   sum_of_other_index_sizes   BIGINT UNSIGNED NOT NULL,
   PRIMARY KEY (database_name, table_name)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS innodb_index_stats (
   database_name        VARCHAR(64) NOT NULL,
   table_name        VARCHAR(64) NOT NULL,
   index_name        VARCHAR(64) NOT NULL,
   stat_timestamp       TIMESTAMP NOT NULL,
   /* there are at least:
   stat_name='size'
   stat_name='n_leaf_pages'
   stat_name='n_diff_pfx%' */
   stat_name         VARCHAR(64) NOT NULL,
   stat_value        BIGINT UNSIGNED NOT NULL,
   sample_size       BIGINT UNSIGNED,
   stat_description     VARCHAR(1024) NOT NULL,
   PRIMARY KEY (database_name, table_name, index_name, stat_name),
   FOREIGN KEY (database_name, table_name)
     REFERENCES innodb_table_stats (database_name, table_name)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

SET SESSION sql_mode=@sql_mode_orig;

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

set @have_pfs= (select count(engine) from information_schema.engines where engine='PERFORMANCE_SCHEMA' and support != 'NO');

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
  "THREAD_ID INTEGER not null,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT'),"
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
  "THREAD_ID INTEGER not null,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT'),"
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
  "THREAD_ID INTEGER not null,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT'),"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "THREAD_ID INTEGER not null,"
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
  "THREAD_ID INTEGER,"
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
-- TABLE MUTEX_INSTANCES
--

SET @cmd="CREATE TABLE performance_schema.mutex_instances("
  "NAME VARCHAR(128) not null,"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "LOCKED_BY_THREAD_ID INTEGER"
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
  "WRITE_LOCKED_BY_THREAD_ID INTEGER,"
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
  "USER CHAR(16) collate utf8_bin default '%' not null,"
  "ROLE CHAR(16) collate utf8_bin default '%' not null"
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
  "OBJECT_TYPE ENUM ('TABLE') not null default 'TABLE',"
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
  "COUNT_WRITE_DELAYED BIGINT unsigned not null,"
  "SUM_TIMER_WRITE_DELAYED BIGINT unsigned not null,"
  "MIN_TIMER_WRITE_DELAYED BIGINT unsigned not null,"
  "AVG_TIMER_WRITE_DELAYED BIGINT unsigned not null,"
  "MAX_TIMER_WRITE_DELAYED BIGINT unsigned not null,"
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
  "THREAD_ID INTEGER not null,"
  "NAME VARCHAR(128) not null,"
  "TYPE VARCHAR(10) not null,"
  "PROCESSLIST_ID INTEGER,"
  "PROCESSLIST_USER VARCHAR(16),"
  "PROCESSLIST_HOST VARCHAR(60),"
  "PROCESSLIST_DB VARCHAR(64),"
  "PROCESSLIST_COMMAND VARCHAR(16),"
  "PROCESSLIST_TIME BIGINT,"
  "PROCESSLIST_STATE VARCHAR(64),"
  "PROCESSLIST_INFO LONGTEXT,"
  "PARENT_THREAD_ID INTEGER,"
  "ROLE VARCHAR(64),"
  "INSTRUMENTED ENUM ('YES', 'NO') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_CURRENT
--

SET @cmd="CREATE TABLE performance_schema.events_stages_current("
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_stages_history("
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_stages_history_long("
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "NESTING_EVENT_ID BIGINT unsigned,"
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_stages_summary_by_thread_by_event_name("
  "THREAD_ID INTEGER not null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_HISTORY
--

SET @cmd="CREATE TABLE performance_schema.events_statements_history("
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_HISTORY_LONG
--

SET @cmd="CREATE TABLE performance_schema.events_statements_history_long("
  "THREAD_ID INTEGER not null,"
  "EVENT_ID BIGINT unsigned not null,"
  "END_EVENT_ID BIGINT unsigned,"
  "EVENT_NAME VARCHAR(128) not null,"
  "SOURCE VARCHAR(64),"
  "TIMER_START BIGINT unsigned,"
  "TIMER_END BIGINT unsigned,"
  "TIMER_WAIT BIGINT unsigned,"
  "LOCK_TIME bigint unsigned not null,"
  "SQL_TEXT LONGTEXT,"
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
  "NESTING_EVENT_TYPE ENUM('STATEMENT', 'STAGE', 'WAIT')"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_THREAD_BY_EVENT_NAME
--

SET @cmd="CREATE TABLE performance_schema.events_statements_summary_by_thread_by_event_name("
  "THREAD_ID INTEGER not null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
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
  "USER CHAR(16) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "CURRENT_CONNECTIONS bigint not null,"
  "TOTAL_CONNECTIONS bigint not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


CREATE TABLE IF NOT EXISTS proxies_priv (Host char(60) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Proxied_host char(60) binary DEFAULT '' NOT NULL, Proxied_user char(16) binary DEFAULT '' NOT NULL, With_grant BOOL DEFAULT 0 NOT NULL, Grantor char(77) DEFAULT '' NOT NULL, Timestamp timestamp, PRIMARY KEY Host (Host,User,Proxied_host,Proxied_user), KEY Grantor (Grantor) ) engine=MyISAM CHARACTER SET utf8 COLLATE utf8_bin comment='User proxy privileges';

-- Remember for later if proxies_priv table already existed
set @had_proxies_priv_table= @@warning_count != 0;
