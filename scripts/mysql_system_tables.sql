-- Copyright (c) 2007, 2017 Oracle and/or its affiliates. All rights reserved.
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

set @have_innodb= (select count(engine) from information_schema.engines where engine='INNODB' and support != 'NO');

-- Tables below are NOT treated as DD tables by MySQL server yet.

SET FOREIGN_KEY_CHECKS= 1;

# Added sql_mode elements and making it as SET, instead of ENUM

--
-- New DD schema end
--

set sql_mode='';
set default_storage_engine=myisam;

CREATE TABLE IF NOT EXISTS db
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
engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Database privileges';

-- Remember for later if db table already existed
set @had_db_table= @@warning_count != 0;

CREATE TABLE IF NOT EXISTS user
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
plugin char(64) DEFAULT 'mysql_native_password' NOT NULL,
authentication_string TEXT,
password_expired ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
password_last_changed timestamp NULL DEFAULT NULL,
password_lifetime smallint unsigned NULL DEFAULT NULL,
account_locked ENUM('N', 'Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Create_role_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
Drop_role_priv enum('N','Y') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL,
PRIMARY KEY Host (Host,User)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Users and global privileges';

CREATE TABLE IF NOT EXISTS default_roles
(
HOST CHAR(60) BINARY DEFAULT '' NOT NULL, USER CHAR(32) BINARY DEFAULT '' NOT NULL,
DEFAULT_ROLE_HOST CHAR(60) BINARY DEFAULT '%' NOT NULL,
DEFAULT_ROLE_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
PRIMARY KEY (HOST, USER, DEFAULT_ROLE_HOST, DEFAULT_ROLE_USER)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Default roles';

CREATE TABLE IF NOT EXISTS role_edges
(
FROM_HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
FROM_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
TO_HOST CHAR(60) BINARY DEFAULT '' NOT NULL,
TO_USER CHAR(32) BINARY DEFAULT '' NOT NULL,
WITH_ADMIN_OPTION ENUM('N', 'Y') COLLATE UTF8_GENERAL_CI DEFAULT 'N' NOT NULL,
PRIMARY KEY (FROM_HOST,FROM_USER,TO_HOST,TO_USER)
) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='Role hierarchy and role grants';

-- Remember for later if user table already existed
set @had_user_table= @@warning_count != 0;


CREATE TABLE IF NOT EXISTS func (  name char(64) binary DEFAULT '' NOT NULL, ret tinyint(1) DEFAULT '0' NOT NULL, dl char(128) DEFAULT '' NOT NULL, type enum ('function','aggregate') COLLATE utf8_general_ci NOT NULL, PRIMARY KEY (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='User defined functions';


CREATE TABLE IF NOT EXISTS plugin ( name varchar(64) DEFAULT '' NOT NULL, dl varchar(128) DEFAULT '' NOT NULL, PRIMARY KEY (name) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_general_ci comment='MySQL plugins';


CREATE TABLE IF NOT EXISTS servers ( Server_name char(64) NOT NULL DEFAULT '', Host char(64) NOT NULL DEFAULT '', Db char(64) NOT NULL DEFAULT '', Username char(64) NOT NULL DEFAULT '', Password char(64) NOT NULL DEFAULT '', Port INT(4) NOT NULL DEFAULT '0', Socket char(64) NOT NULL DEFAULT '', Wrapper char(64) NOT NULL DEFAULT '', Owner char(64) NOT NULL DEFAULT '', PRIMARY KEY (Server_name)) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='MySQL Foreign Servers table';


CREATE TABLE IF NOT EXISTS tables_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Table privileges';

CREATE TABLE IF NOT EXISTS columns_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, Column_priv set('Select','Insert','Update','References') COLLATE utf8_general_ci DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Column privileges';


CREATE TABLE IF NOT EXISTS help_topic ( help_topic_id int unsigned not null, name char(64) not null, help_category_id smallint unsigned not null, description text not null, example text not null, url text not null, primary key (help_topic_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help topics';


CREATE TABLE IF NOT EXISTS help_category ( help_category_id smallint unsigned not null, name  char(64) not null, parent_category_id smallint unsigned null, url text not null, primary key (help_category_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help categories';


CREATE TABLE IF NOT EXISTS help_relation ( help_topic_id int unsigned not null, help_keyword_id  int unsigned not null, primary key (help_keyword_id, help_topic_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='keyword-topic relation';


CREATE TABLE IF NOT EXISTS help_keyword (   help_keyword_id  int unsigned not null, name char(64) not null, primary key (help_keyword_id), unique index (name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8 comment='help keywords';


CREATE TABLE IF NOT EXISTS time_zone_name (   Name char(64) NOT NULL, Time_zone_id int unsigned NOT NULL, PRIMARY KEY Name (Name) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone names';


CREATE TABLE IF NOT EXISTS time_zone (   Time_zone_id int unsigned NOT NULL auto_increment, Use_leap_seconds enum('Y','N') COLLATE utf8_general_ci DEFAULT 'N' NOT NULL, PRIMARY KEY TzId (Time_zone_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zones';


CREATE TABLE IF NOT EXISTS time_zone_transition (   Time_zone_id int unsigned NOT NULL, Transition_time bigint signed NOT NULL, Transition_type_id int unsigned NOT NULL, PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transitions';


CREATE TABLE IF NOT EXISTS time_zone_transition_type (   Time_zone_id int unsigned NOT NULL, Transition_type_id int unsigned NOT NULL, Offset int signed DEFAULT 0 NOT NULL, Is_DST tinyint unsigned DEFAULT 0 NOT NULL, Abbreviation char(8) DEFAULT '' NOT NULL, PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Time zone transition types';


CREATE TABLE IF NOT EXISTS time_zone_leap_second (   Transition_time bigint signed NOT NULL, Correction int signed NOT NULL, PRIMARY KEY TranTime (Transition_time) ) engine=INNODB STATS_PERSISTENT=0 CHARACTER SET utf8   comment='Leap seconds information for time zones';


CREATE TABLE IF NOT EXISTS procs_priv ( Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Routine_name char(64) COLLATE utf8_general_ci DEFAULT '' NOT NULL, Routine_type enum('FUNCTION','PROCEDURE') NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Proc_priv set('Execute','Alter Routine','Grant') COLLATE utf8_general_ci DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY (Host,Db,User,Routine_name,Routine_type), KEY Grantor (Grantor) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin   comment='Procedure privileges';

-- Create general_log
CREATE TABLE IF NOT EXISTS general_log (event_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL, server_id INTEGER UNSIGNED NOT NULL, command_type VARCHAR(64) NOT NULL, argument MEDIUMBLOB NOT NULL) engine=CSV CHARACTER SET utf8 comment="General log";

-- Create slow_log
CREATE TABLE IF NOT EXISTS slow_log (start_time TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6) ON UPDATE CURRENT_TIMESTAMP(6), user_host MEDIUMTEXT NOT NULL, query_time TIME(6) NOT NULL, lock_time TIME(6) NOT NULL, rows_sent INTEGER NOT NULL, rows_examined INTEGER NOT NULL, db VARCHAR(512) NOT NULL, last_insert_id INTEGER NOT NULL, insert_id INTEGER NOT NULL, server_id INTEGER UNSIGNED NOT NULL, sql_text MEDIUMBLOB NOT NULL, thread_id BIGINT(21) UNSIGNED NOT NULL) engine=CSV CHARACTER SET utf8 comment="Slow log";


CREATE TABLE IF NOT EXISTS component ( component_id int unsigned NOT NULL AUTO_INCREMENT, component_group_id int unsigned NOT NULL, component_urn text NOT NULL, PRIMARY KEY (component_id)) engine=INNODB DEFAULT CHARSET=utf8 COMMENT 'Components';

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

INSERT IGNORE INTO server_cost VALUES
  ("row_evaluate_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

INSERT IGNORE INTO server_cost VALUES
  ("key_compare_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

INSERT IGNORE INTO server_cost VALUES
  ("memory_temptable_create_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

INSERT IGNORE INTO server_cost VALUES
  ("memory_temptable_row_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

INSERT IGNORE INTO server_cost VALUES
  ("disk_temptable_create_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

INSERT IGNORE INTO server_cost VALUES
  ("disk_temptable_row_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);

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

INSERT IGNORE INTO engine_cost VALUES
  ("default", 0, "memory_block_read_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);
INSERT IGNORE INTO engine_cost VALUES
  ("default", 0, "io_block_read_cost", DEFAULT, CURRENT_TIMESTAMP, DEFAULT);


--
-- Column statistics
--

CREATE TABLE IF NOT EXISTS column_stats (
  database_name VARCHAR(64) NOT NULL,
  table_name VARCHAR(64) NOT NULL,
  column_name VARCHAR(64) NOT NULL,
  histogram JSON NOT NULL,
  PRIMARY KEY (database_name, table_name, column_name)
) ENGINE=InnoDB CHARACTER SET=utf8 COLLATE=utf8_bin
COMMENT="Column statistics";


--
--
-- INFORMATION SCHEMA VIEWS INSTALLATION
--

-- Set explicit collation for columns that use utf8_tolower_ci.
-- This is required to enable optimizer allow comparison between
-- utf8_tolower_ci and utf8_general_ci columns and to pick right index.

SET @collate_tolower= (SELECT IF(@@lower_case_table_names = 0,
                                 '', 'COLLATE utf8_tolower_ci'));

--
-- INFORMATION_SCHEMA.COLLATIONS
--
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.COLLATIONS AS
  SELECT col.name AS COLLATION_NAME,
         cs.name AS CHARACTER_SET_NAME,
         col.id AS ID,
         IF(EXISTS(SELECT * FROM mysql.character_sets
                            WHERE mysql.character_sets.default_collation_id= col.id),
            'Yes','') AS IS_DEFAULT,
         IF(col.is_compiled,'Yes','') AS IS_COMPILED,
         col.sort_length AS SORTLEN
  FROM mysql.collations col JOIN mysql.character_sets cs ON col.character_set_id=cs.id;

--
-- INFORMATION_SCHEMA.CHARACTER_SETS
--
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.CHARACTER_SETS as
  SELECT cs.name AS CHARACTER_SET_NAME,
         col.name AS DEFAULT_COLLATE_NAME,
         cs.comment AS DESCRIPTION,
         cs.mb_max_length AS MAXLEN
  FROM mysql.character_sets cs JOIN mysql.collations col ON cs.default_collation_id = col.id;

--
-- INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY
--
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.COLLATION_CHARACTER_SET_APPLICABILITY AS
  SELECT col.name AS COLLATION_NAME,
         cs.name AS CHARACTER_SET_NAME
  FROM mysql.character_sets cs JOIN mysql.collations col ON cs.id = col.character_set_id;

--
-- INFORMATION_SCHEMA.ST_SPATIAL_REFERENCE_SYSTEMS
--
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.ST_SPATIAL_REFERENCE_SYSTEMS AS
  SELECT name AS SRS_NAME,
         id AS SRS_ID,
         organization AS ORGANIZATION,
         organization_coordsys_id AS ORGANIZATION_COORDSYS_ID,
         definition AS DEFINITION,
         description AS DESCRIPTION
  FROM mysql.st_spatial_reference_systems;

--
-- INFORMATION_SCHEMA.SCHEMATA
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.SCHEMATA AS
  SELECT cat.name ", @collate_tolower, " AS CATALOG_NAME,
    sch.name ", @collate_tolower, " AS SCHEMA_NAME,
    cs.name AS DEFAULT_CHARACTER_SET_NAME,
    col.name AS DEFAULT_COLLATION_NAME,
    NULL AS SQL_PATH
  FROM mysql.schemata sch JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations col ON sch.default_collation_id = col.id
       JOIN mysql.character_sets cs ON col.character_set_id= cs.id
  WHERE CAN_ACCESS_DATABASE(sch.name)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- INFORMATION_SCHEMA.TABLES
--
-- There are two definitions of information_schema.tables.
-- 1. INFORMATION_SCHEMA.TABLES view which picks dynamic column
--    statistics from mysql.table_stats which gets populated when
--    we execute 'anaylze table' command.
--
-- 2. INFORMATION_SCHEMA.TABLES_DYNAMIC view which retrieves dynamic
--    column statistics using a internal UDF which opens the user
--    table and reads dynamic table statistics.
--
-- MySQL server uses definition 1) by default. The session variable
-- information_schema_stats=latest would enable use of definition 2).
--

SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.TABLES AS
  SELECT cat.name ", @collate_tolower, " AS TABLE_CATALOG,
    sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
    tbl.name ", @collate_tolower, " AS TABLE_NAME,
    tbl.type AS TABLE_TYPE,
    IF(tbl.type = 'BASE TABLE', tbl.engine, NULL) AS ENGINE,
    IF(tbl.type = 'VIEW', NULL, 10 /* FRM_VER_TRUE_VARCHAR */) AS VERSION,
    tbl.row_format AS ROW_FORMAT,
    stat.table_rows AS TABLE_ROWS,
    stat.avg_row_length AS AVG_ROW_LENGTH,
    stat.data_length AS DATA_LENGTH,
    stat.max_data_length AS MAX_DATA_LENGTH,
    stat.index_length AS INDEX_LENGTH,
    stat.data_free AS DATA_FREE,
    stat.auto_increment AS AUTO_INCREMENT,
    tbl.created AS CREATE_TIME,
    stat.update_time AS UPDATE_TIME,
    stat.check_time AS CHECK_TIME,
    col.name AS TABLE_COLLATION,
    stat.checksum AS CHECKSUM,
    IF (tbl.type = 'VIEW', NULL, 
        GET_DD_CREATE_OPTIONS(tbl.options,
          IF(IFNULL(tbl.partition_expression,'NOT_PART_TBL')='NOT_PART_TBL', 0, 1)))
        AS CREATE_OPTIONS,
    INTERNAL_GET_COMMENT_OR_ERROR(sch.name, tbl.name, tbl.type, tbl.options, tbl.comment)
       AS TABLE_COMMENT
  FROM mysql.tables tbl JOIN mysql.schemata sch ON tbl.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       LEFT JOIN mysql.collations col ON tbl.collation_id=col.id
       LEFT JOIN mysql.table_stats stat ON tbl.name=stat.table_name
       AND sch.name=stat.schema_name
  WHERE CAN_ACCESS_TABLE(sch.name, tbl.name) AND NOT tbl.hidden");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.TABLES_DYNAMIC AS
  SELECT cat.name ", @collate_tolower, " AS TABLE_CATALOG,
    sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
    tbl.name ", @collate_tolower, " AS TABLE_NAME,
    tbl.type AS TABLE_TYPE,
    IF(tbl.type = 'BASE TABLE', tbl.engine, NULL) AS ENGINE,
    IF(tbl.type = 'VIEW', NULL, 10 /* FRM_VER_TRUE_VARCHAR */) AS VERSION,
    tbl.row_format AS ROW_FORMAT,
    INTERNAL_TABLE_ROWS(sch.name, tbl.name,
                        IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                        tbl.se_private_id) AS TABLE_ROWS,
    INTERNAL_AVG_ROW_LENGTH(sch.name, tbl.name,
                            IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                            tbl.se_private_id) AS AVG_ROW_LENGTH,
    INTERNAL_DATA_LENGTH(sch.name, tbl.name,
                         IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                         tbl.se_private_id) AS DATA_LENGTH,
    INTERNAL_MAX_DATA_LENGTH(sch.name, tbl.name,
                             IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                             tbl.se_private_id) AS MAX_DATA_LENGTH,
    INTERNAL_INDEX_LENGTH(sch.name, tbl.name,
                          IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                          tbl.se_private_id) AS INDEX_LENGTH,
    INTERNAL_DATA_FREE(sch.name, tbl.name,
                       IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                       tbl.se_private_id) AS DATA_FREE,
    INTERNAL_AUTO_INCREMENT(sch.name, tbl.name,
                            IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                            tbl.se_private_id) AS AUTO_INCREMENT,
    tbl.created AS CREATE_TIME,
    INTERNAL_UPDATE_TIME(sch.name, tbl.name,
                         IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                         tbl.se_private_id) AS UPDATE_TIME,
    INTERNAL_CHECK_TIME(sch.name, tbl.name,
                        IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                        tbl.se_private_id) AS CHECK_TIME,
    col.name AS TABLE_COLLATION,
    INTERNAL_CHECKSUM(sch.name, tbl.name,
                      IF(IFNULL(tbl.partition_type,'')='',tbl.engine,''),
                      tbl.se_private_id) AS CHECKSUM,
    IF (tbl.type = 'VIEW', NULL, 
        GET_DD_CREATE_OPTIONS(tbl.options,
          IF(IFNULL(tbl.partition_expression,'NOT_PART_TBL')='NOT_PART_TBL', 0, 1)))
        AS CREATE_OPTIONS,
    INTERNAL_GET_COMMENT_OR_ERROR(sch.name, tbl.name, tbl.type, tbl.options, tbl.comment)
       AS TABLE_COMMENT
  FROM mysql.tables tbl JOIN mysql.schemata sch ON tbl.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       LEFT JOIN mysql.collations col ON tbl.collation_id=col.id
  WHERE CAN_ACCESS_TABLE(sch.name, tbl.name) AND NOT tbl.hidden");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- INFORMATION_SCHEMA.COLUMNS
--

SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.COLUMNS AS
  SELECT
    cat.name ", @collate_tolower, " AS TABLE_CATALOG,
    sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
    tbl.name ", @collate_tolower, " AS TABLE_NAME,
    col.name COLLATE utf8_tolower_ci AS COLUMN_NAME,
    col.ordinal_position AS ORDINAL_POSITION,
    col.default_value_utf8 AS COLUMN_DEFAULT,
    IF (col.is_nullable = 1, 'YES','NO') AS IS_NULLABLE,
    SUBSTRING_INDEX(SUBSTRING_INDEX(col.column_type_utf8, '(', 1), ' ', 1) AS DATA_TYPE,
    INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
    INTERNAL_DD_CHAR_LENGTH(col.type, col.char_length, coll.name, 1) AS CHARACTER_OCTET_LENGTH,
    IF (col.numeric_precision = 0, NULL, col.numeric_precision) AS NUMERIC_PRECISION,
    IF (col.numeric_scale = 0 && col.numeric_precision = 0, NULL, col.numeric_scale) AS NUMERIC_SCALE,
    col.datetime_precision AS DATETIME_PRECISION,
    CASE col.type
      WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, cs.name))
      WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, cs.name))
      ELSE NULL
    END AS CHARACTER_SET_NAME,
    CASE col.type
      WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, coll.name))
      WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, coll.name))
      ELSE NULL
    END AS COLLATION_NAME,
    col.column_type_utf8 AS COLUMN_TYPE,
    col.column_key AS COLUMN_KEY,
    IF(IFNULL(col.generation_expression_utf8,'IS_NOT_GC')='IS_NOT_GC',
       IF (col.is_auto_increment=TRUE,
            CONCAT(IFNULL(CONCAT('on update ', col.update_option, ' '),''),
                    'auto_increment'),
           IFNULL(CONCAT('on update ', col.update_option),'')),
      IF(col.is_virtual, 'VIRTUAL GENERATED', 'STORED GENERATED')) AS EXTRA,
    GET_DD_COLUMN_PRIVILEGES(sch.name, tbl.name, col.name) AS `PRIVILEGES`,
    IFNULL(col.comment, '') AS COLUMN_COMMENT,
    IFNULL(col.generation_expression_utf8, '') AS GENERATION_EXPRESSION
  FROM mysql.columns col JOIN mysql.tables tbl ON col.table_id=tbl.id
       JOIN mysql.schemata sch ON tbl.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations coll ON col.collation_id=coll.id
       JOIN mysql.character_sets cs ON coll.character_set_id= cs.id
  WHERE INTERNAL_GET_VIEW_WARNING_OR_ERROR(sch.name, tbl.name, tbl.type, tbl.options) AND
        CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name) AND NOT tbl.hidden");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS
--
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.ST_GEOMETRY_COLUMNS AS
  SELECT TABLE_CATALOG,
         TABLE_SCHEMA,
         TABLE_NAME,
         COLUMN_NAME,
         NULL AS SRS_NAME,
         NULL AS SRS_ID,
	 DATA_TYPE AS GEOMETRY_TYPE_NAME
  FROM information_schema.COLUMNS
  WHERE DATA_TYPE IN (
    'geometry',
    'point',
    'linestring',
    'polygon',
    'multipoint',
    'multilinestring',
    'multipolygon',
    'geometrycollection');

--
-- INFORMATION_SCHEMA.STATISTICS
--
-- There are two definitions of information_schema.statistics.
-- 1. INFORMATION_SCHEMA.STATISTICS view which picks dynamic column
--    statistics from mysql.index_stats which gets populated when
--    we execute 'anaylze table' command.
--
-- 2. INFORMATION_SCHEMA.STATISTICS_DYNAMIC view which retrieves dynamic
--    column statistics using a internal UDF which opens the user
--    table and reads dynamic table statistics.
--
-- MySQL server uses definition 1) by default. The session variable
-- information_schema_stats=latest would enable use of definition 2).
--

SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.STATISTICS_BASE AS
  (SELECT cat.name ", @collate_tolower, " AS TABLE_CATALOG,
    sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
    tbl.name ", @collate_tolower, " AS TABLE_NAME,
    IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE','0','1') AS NON_UNIQUE,
    sch.name ", @collate_tolower, " AS INDEX_SCHEMA,
    idx.name COLLATE utf8_tolower_ci AS INDEX_NAME,
    icu.ordinal_position AS SEQ_IN_INDEX,
    col.name COLLATE utf8_tolower_ci AS COLUMN_NAME,
    CASE WHEN icu.order = 'DESC' THEN 'D'
         WHEN icu.order = 'ASC'  THEN 'A'
         ELSE NULL END AS COLLATION,
    GET_DD_INDEX_SUB_PART_LENGTH(icu.length, col.type, col.char_length,
                                 col.collation_id, idx.options) AS SUB_PART,
    NULL AS PACKED,
    if (col.is_nullable = 1, 'YES','') AS NULLABLE,
    CASE WHEN idx.type = 'SPATIAL' THEN 'SPATIAL'
         WHEN idx.algorithm = 'SE_PRIVATE' THEN ''
         ELSE idx.algorithm END AS INDEX_TYPE,
    IF (idx.type = 'PRIMARY' OR idx.type = 'UNIQUE',
        '',IF(INTERNAL_KEYS_DISABLED(tbl.options),'disabled', ''))
      AS COMMENT,
    idx.comment AS INDEX_COMMENT,
    IF (idx.is_visible, 'YES', 'NO') AS IS_VISIBLE,
    idx.ordinal_position AS INDEX_ORDINAL_POSITION,
    icu.ordinal_position AS COLUMN_ORDINAL_POSITION,
    tbl.engine AS ENGINE,
    tbl.se_private_id AS SE_PRIVATE_ID,
    idx.hidden OR icu.hidden AS IS_HIDDEN
  FROM mysql.index_column_usage icu JOIN mysql.indexes idx ON idx.id=icu.index_id
    JOIN mysql.tables tbl ON idx.table_id=tbl.id
    JOIN mysql.columns col ON icu.column_id=col.id
    JOIN mysql.schemata sch ON tbl.schema_id=sch.id
    JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
    JOIN mysql.collations coll ON tbl.collation_id=coll.id
  WHERE CAN_ACCESS_TABLE(sch.name, tbl.name) AND NOT tbl.hidden)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.STATISTICS AS
 (SELECT TABLE_CATALOG,
    TABLE_SCHEMA,
    sb.TABLE_NAME AS `TABLE_NAME`,
    NON_UNIQUE,
    INDEX_SCHEMA,
    sb.INDEX_NAME AS `INDEX_NAME`,
    SEQ_IN_INDEX,
    sb.COLUMN_NAME AS `COLUMN_NAME`,
    COLLATION,
    stat.cardinality AS CARDINALITY,
    SUB_PART,
    PACKED,
    NULLABLE,
    INDEX_TYPE,
    COMMENT,
    INDEX_COMMENT,
    IS_VISIBLE
  FROM information_schema.STATISTICS_BASE sb
    LEFT JOIN mysql.index_stats stat
                 ON sb.table_name=stat.table_name
                and sb.table_schema=stat.schema_name
                and sb.index_name=stat.index_name
                and sb.column_name=stat.column_name);

CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.STATISTICS_DYNAMIC AS
 (SELECT TABLE_CATALOG,
    TABLE_SCHEMA,
    TABLE_NAME,
    NON_UNIQUE,
    INDEX_SCHEMA,
    INDEX_NAME,
    SEQ_IN_INDEX,
    COLUMN_NAME,
    COLLATION,
    INTERNAL_INDEX_COLUMN_CARDINALITY(TABLE_SCHEMA, TABLE_NAME, INDEX_NAME,
                                      INDEX_ORDINAL_POSITION,
                                      COLUMN_ORDINAL_POSITION,
                                      ENGINE, SE_PRIVATE_ID, IS_HIDDEN)
      AS CARDINALITY,
    SUB_PART,
    PACKED,
    NULLABLE,
    INDEX_TYPE,
    COMMENT,
    INDEX_COMMENT,
    IS_VISIBLE
  FROM INFORMATION_SCHEMA.STATISTICS_BASE);

--
-- INFORMATION_SCHEMA.TABLE_CONSTRAINTS
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.TABLE_CONSTRAINTS AS
  (SELECT cat.name ", @collate_tolower, " AS CONSTRAINT_CATALOG,
          sch.name ", @collate_tolower, " AS CONSTRAINT_SCHEMA,
          CONVERT(idx.name USING utf8) AS CONSTRAINT_NAME,
          sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
          tbl.name ", @collate_tolower, " AS TABLE_NAME,
          IF (idx.type='PRIMARY', 'PRIMARY KEY', idx.type) AS CONSTRAINT_TYPE
    FROM mysql.indexes idx JOIN mysql.tables tbl ON idx.table_id = tbl.id
         JOIN mysql.schemata sch ON tbl.schema_id= sch.id
         JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
         AND idx.type IN ('PRIMARY', 'UNIQUE')
    WHERE CAN_ACCESS_TABLE(sch.name, tbl.name) AND NOT tbl.hidden)
  UNION
  (SELECT cat.name ", @collate_tolower, " AS CONSTRAINT_CATALOG,
          sch.name ", @collate_tolower, " AS CONSTRAINT_SCHEMA,
          CONVERT(fk.name USING utf8)  AS CONSTRAINT_NAME,
          sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
          tbl.name ", @collate_tolower, " AS TABLE_NAME,
          'FOREIGN KEY' AS CONSTRAINT_TYPE
    FROM mysql.foreign_keys fk JOIN mysql.tables tbl ON fk.table_id = tbl.id
         JOIN mysql.schemata sch ON fk.schema_id= sch.id
         JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
    WHERE CAN_ACCESS_TABLE(sch.name, tbl.name) AND NOT tbl.hidden)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- INFORMATION_SCHEMA.KEY_COLUMN_USAGE
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.KEY_COLUMN_USAGE AS
  (SELECT cat.name ", @collate_tolower, " AS CONSTRAINT_CATALOG,
     sch.name ", @collate_tolower, " AS CONSTRAINT_SCHEMA,
     CONVERT(idx.name USING utf8) AS CONSTRAINT_NAME,
     cat.name ", @collate_tolower, " AS TABLE_CATALOG,
     sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
     tbl.name ", @collate_tolower, " AS TABLE_NAME,
     col.name COLLATE utf8_tolower_ci AS COLUMN_NAME,
     icu.ordinal_position AS ORDINAL_POSITION,
     NULL AS POSITION_IN_UNIQUE_CONSTRAINT,
     NULL AS REFERENCED_TABLE_SCHEMA,
     NULL AS REFERENCED_TABLE_NAME,
     NULL AS REFERENCED_COLUMN_NAME
   FROM mysql.indexes idx JOIN mysql.tables tbl ON idx.table_id = tbl.id
     JOIN mysql.schemata sch ON tbl.schema_id= sch.id
     JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
     JOIN mysql.index_column_usage icu ON icu.index_id=idx.id
     JOIN mysql.columns col ON icu.column_id=col.id
     AND idx.type IN ('PRIMARY', 'UNIQUE')
   WHERE CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name) AND NOT tbl.hidden)
  UNION
  (SELECT cat.name ", @collate_tolower, " AS CONSTRAINT_CATALOG,
     sch.name ", @collate_tolower, " AS CONSTRAINT_SCHEMA,
     CONVERT(fk.name USING utf8) AS CONSTRAINT_NAME,
     cat.name ", @collate_tolower, " AS TABLE_CATALOG,
     sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
     tbl.name ", @collate_tolower, " AS TABLE_NAME,
     col.name COLLATE utf8_tolower_ci AS COLUMN_NAME,
     fkcu.ordinal_position  AS ORDINAL_POSITION,
     fkcu.ordinal_position AS POSITION_IN_UNIQUE_CONSTRAINT,
     fk.referenced_table_schema AS REFERENCED_TABLE_SCHEMA,
     fk.referenced_table_name AS REFERENCED_TABLE_NAME,
     fkcu.referenced_column_name AS REFERENCED_COLUMN_NAME
   FROM mysql.foreign_keys fk JOIN mysql.tables tbl ON fk.table_id = tbl.id
     JOIN mysql.foreign_key_column_usage fkcu ON fkcu.foreign_key_id=fk.id
     JOIN mysql.schemata sch ON fk.schema_id= sch.id
     JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
     JOIN mysql.columns col ON fkcu.column_id=col.id
   WHERE CAN_ACCESS_COLUMN(sch.name, tbl.name, col.name) AND NOT tbl.hidden)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- INFORMATION_SCHEMA.VIEWS
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.VIEWS AS
  SELECT cat.name ", @collate_tolower, " AS TABLE_CATALOG,
    sch.name ", @collate_tolower, " AS TABLE_SCHEMA,
    vw.name ", @collate_tolower, " AS TABLE_NAME,
    IF(CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options) = TRUE,
       vw.view_definition_utf8, '') AS VIEW_DEFINITION,
    vw.view_check_option AS CHECK_OPTION,
    vw.view_is_updatable AS IS_UPDATABLE,
    vw.view_definer AS DEFINER,
    IF (vw.view_security_type = 'DEFAULT', 'DEFINER', vw.view_security_type)
      AS SECURITY_TYPE,
    cs.name AS CHARACTER_SET_CLIENT,
    conn_coll.name AS COLLATION_CONNECTION
  FROM mysql.tables vw JOIN mysql.schemata sch ON vw.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations conn_coll ON conn_coll.id= vw.view_connection_collation_id
       JOIN mysql.collations client_coll ON client_coll.id= vw.view_client_collation_id
       JOIN mysql.character_sets cs ON cs.id= client_coll.character_set_id
  WHERE vw.type = 'VIEW' AND CAN_ACCESS_TABLE(sch.name, vw.name)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- INFORMATION_SCHEMA.TRIGGERS
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.TRIGGERS AS
SELECT cat.name AS TRIGGER_CATALOG,
       sch.name AS TRIGGER_SCHEMA,
       trg.name AS TRIGGER_NAME,
       trg.event_type AS EVENT_MANIPULATION,
       cat.name AS EVENT_OBJECT_CATALOG,
       sch.name AS EVENT_OBJECT_SCHEMA,
       tbl.name AS EVENT_OBJECT_TABLE,
       trg.action_order AS ACTION_ORDER,
       NULL AS ACTION_CONDITION,
       trg.action_statement_utf8 AS ACTION_STATEMENT,
       'ROW' AS ACTION_ORIENTATION,
       trg.action_timing AS ACTION_TIMING,
       NULL AS ACTION_REFERENCE_OLD_TABLE,
       NULL AS ACTION_REFERENCE_NEW_TABLE,
       'OLD' AS ACTION_REFERENCE_OLD_ROW,
       'NEW' AS ACTION_REFERENCE_NEW_ROW,
       trg.created AS CREATED,
       trg.sql_mode AS SQL_MODE,
       trg.definer AS DEFINER,
       cs_client.name AS CHARACTER_SET_CLIENT,
       coll_conn.name AS COLLATION_CONNECTION,
       coll_db.name AS DATABASE_COLLATION
  FROM mysql.triggers trg JOIN mysql.tables tbl ON tbl.id=trg.table_id
       JOIN mysql.schemata sch ON tbl.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations coll_client ON coll_client.id=trg.client_collation_id
       JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
       JOIN mysql.collations coll_conn ON coll_conn.id=trg.connection_collation_id
       JOIN mysql.collations coll_db ON coll_db.id=trg.schema_collation_id
  WHERE tbl.type != 'VIEW' AND CAN_ACCESS_TRIGGER(sch.name, tbl.name) AND NOT tbl.hidden");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- INFORMATION_SCHEMA.ROUTINES
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.ROUTINES AS
SELECT rtn.name AS SPECIFIC_NAME,
    cat.name AS ROUTINE_CATALOG,
    sch.name AS ROUTINE_SCHEMA,
    rtn.name AS ROUTINE_NAME,
    rtn.type AS ROUTINE_TYPE,
    IF(rtn.type = 'PROCEDURE', '', 
       SUBSTRING_INDEX(SUBSTRING_INDEX(
                         rtn.result_data_type_utf8, '(', 1), ' ', 1)) AS DATA_TYPE,
    INTERNAL_DD_CHAR_LENGTH(
      rtn.result_data_type,
      rtn.result_char_length, coll_result.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
    INTERNAL_DD_CHAR_LENGTH(
      rtn.result_data_type,
      rtn.result_char_length, coll_result.name, 1) AS CHARACTER_OCTET_LENGTH,
    rtn.result_numeric_precision AS NUMERIC_PRECISION,
    rtn.result_numeric_scale AS NUMERIC_SCALE,
    rtn.result_datetime_precision AS DATETIME_PRECISION,
    CASE rtn.result_data_type
      WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      WHEN 'MYSQL_TYPE_SET' THEN (IF (cs_result.name='binary',NULL, cs_result.name))
      ELSE NULL
    END AS CHARACTER_SET_NAME,
    CASE rtn.result_data_type
      WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      WHEN 'MYSQL_TYPE_SET' THEN (IF (cs_result.name='binary',NULL, coll_result.name))
      ELSE NULL
    END AS COLLATION_NAME,
    IF(rtn.type = 'PROCEDURE', NULL, rtn.result_data_type_utf8) AS DTD_IDENTIFIER,
    'SQL' AS ROUTINE_BODY,
    IF (CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, TRUE),
        rtn.definition_utf8, NULL) AS ROUTINE_DEFINITION,
    NULL AS EXTERNAL_NAME,
    NULL AS EXTERNAL_LANGUAGE,
    'SQL' AS PARAMETER_STYLE,
    IF(rtn.is_deterministic=0, 'NO', 'YES') AS IS_DETERMINISTIC,
    rtn.sql_data_access AS SQL_DATA_ACCESS,
    NULL AS SQL_PATH,
    rtn.security_type AS SECURITY_TYPE,
    rtn.created AS CREATED,
    rtn.last_altered AS LAST_ALTERED,
    rtn.sql_mode AS SQL_MODE,
    rtn.comment AS ROUTINE_COMMENT,
    rtn.definer AS DEFINER,
    cs_client.name AS CHARACTER_SET_CLIENT,
    coll_conn.name AS COLLATION_CONNECTION,
    coll_db.name AS DATABASE_COLLATION
  FROM mysql.routines rtn
       JOIN mysql.schemata sch ON rtn.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations coll_client ON coll_client.id=rtn.client_collation_id
       JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
       JOIN mysql.collations coll_conn ON coll_conn.id=rtn.connection_collation_id
       JOIN mysql.collations coll_db ON coll_db.id=rtn.schema_collation_id
       LEFT JOIN mysql.collations coll_result ON coll_result.id=rtn.result_collation_id
       LEFT JOIN mysql.character_sets cs_result ON cs_result.id=coll_result.character_set_id
  WHERE CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- INFORMATION_SCHEMA.PARAMETERS
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.PARAMETERS AS
SELECT
  cat.name AS SPECIFIC_CATALOG,
  sch.name AS SPECIFIC_SCHEMA,
  rtn.name AS SPECIFIC_NAME,
  IF (rtn.type = 'FUNCTION',
      prm.ordinal_position-1,
      prm.ordinal_position) AS ORDINAL_POSITION,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.mode) AS PARAMETER_MODE,
  IF (rtn.type = 'FUNCTION' AND prm.ordinal_position = 1, NULL, prm.name) AS PARAMETER_NAME,
  SUBSTRING_INDEX(SUBSTRING_INDEX(prm.data_type_utf8, '(', 1), ' ', 1) AS DATA_TYPE,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 0) AS CHARACTER_MAXIMUM_LENGTH,
  INTERNAL_DD_CHAR_LENGTH(prm.data_type, prm.char_length, col.name, 1) AS CHARACTER_OCTET_LENGTH,
  prm.numeric_precision AS NUMERIC_PRECISION,
  IF(ISNULL(prm.numeric_precision), NULL, IFNULL(prm.numeric_scale, 0))   AS NUMERIC_SCALE,
  prm.datetime_precision AS DATETIME_PRECISION,
  CASE prm.data_type
    WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, cs.name))
    WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, cs.name))
    ELSE NULL
  END AS CHARACTER_SET_NAME,
  CASE prm.data_type
    WHEN 'MYSQL_TYPE_STRING' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_VAR_STRING' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_VARCHAR' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_TINY_BLOB' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_MEDIUM_BLOB' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_BLOB' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_LONG_BLOB' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_ENUM' THEN (IF (cs.name='binary',NULL, col.name))
    WHEN 'MYSQL_TYPE_SET' THEN (IF (cs.name='binary',NULL, col.name))
    ELSE NULL
  END AS COLLATION_NAME,
  prm.data_type_utf8 AS DTD_IDENTIFIER,
  rtn.type AS ROUTINE_TYPE
  FROM mysql.parameters prm JOIN mysql.routines rtn ON prm.routine_id=rtn.id
       JOIN mysql.schemata sch ON rtn.schema_id=sch.id
       JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
       JOIN mysql.collations col ON prm.collation_id=col.id
       JOIN mysql.character_sets cs ON col.character_set_id=cs.id
  WHERE CAN_ACCESS_ROUTINE(sch.name, rtn.name, rtn.type, rtn.definer, FALSE)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


--
-- INFORMATION_SCHEMA.EVENTS
--
SET @str=CONCAT("
CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.EVENTS AS
SELECT
  cat.name AS EVENT_CATALOG,
  sch.name AS EVENT_SCHEMA,
  evt.name AS EVENT_NAME,
  evt.definer AS DEFINER,
  evt.time_zone AS TIME_ZONE,
  'SQL' AS EVENT_BODY,
  evt.definition_utf8 AS EVENT_DEFINITION,
  IF (ISNULL(evt.interval_value),'ONE TIME','RECURRING') AS EVENT_TYPE,
  CONVERT_TZ(evt.execute_at,'+00:00', evt.time_zone) AS EXECUTE_AT,
  evt.interval_value AS INTERVAL_VALUE,
  evt.interval_field AS INTERVAL_FIELD,
  evt.sql_mode AS SQL_MODE,
  CONVERT_TZ(evt.starts,'+00:00', evt.time_zone) AS STARTS,
  CONVERT_TZ(evt.ends,'+00:00', evt.time_zone) AS ENDS,
  evt.status AS STATUS,
  IF (evt.on_completion='DROP', 'NOT PRESERVE', 'PRESERVE') AS ON_COMPLETION,
  evt.created AS CREATED,
  evt.last_altered AS LAST_ALTERED,
  evt.last_executed AS LAST_EXECUTED,
  evt.comment AS EVENT_COMMENT,
  evt.originator AS ORIGINATOR,
  cs_client.name AS CHARACTER_SET_CLIENT,
  coll_conn.name AS COLLATION_CONNECTION,
  coll_db.name AS DATABASE_COLLATION
FROM mysql.events evt
     JOIN mysql.schemata sch ON evt.schema_id=sch.id
     JOIN mysql.catalogs cat ON cat.id=sch.catalog_id
     JOIN mysql.collations coll_client ON coll_client.id=evt.client_collation_id
     JOIN mysql.character_sets cs_client ON cs_client.id=coll_client.character_set_id
     JOIN mysql.collations coll_conn ON coll_conn.id=evt.connection_collation_id
     JOIN mysql.collations coll_db ON coll_db.id=evt.schema_collation_id
WHERE CAN_ACCESS_EVENT(sch.name)");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

-- END OF INFORMATION SCHEMA INSTALLATION


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

SET @broken_routines = (select count(*) from information_schema.routines where routine_schema='performance_schema');

SET @broken_events = (select count(*) from information_schema.events where event_schema='performance_schema');

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
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (NAME) USING HASH"
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
  "FLAGS INTEGER unsigned,"
  "PRIMARY KEY (THREAD_ID, EVENT_ID)"
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
  "FLAGS INTEGER unsigned,"
  "PRIMARY KEY (THREAD_ID, EVENT_ID)"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN),"
  "KEY (EVENT_NAME)"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY (HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY (USER, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
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
  "OPEN_COUNT INTEGER unsigned not null,"
  "PRIMARY KEY (FILE_NAME) USING HASH,"
  "KEY (EVENT_NAME) USING HASH"
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
  "MAX_TIMER_MISC BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
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
  "MAX_TIMER_MISC BIGINT unsigned not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (FILE_NAME) USING HASH,"
  "KEY (EVENT_NAME) USING HASH"
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
  "STATE ENUM('IDLE','ACTIVE') not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (THREAD_ID) USING HASH,"
  "KEY (SOCKET_ID) USING HASH,"
  "KEY (IP, PORT) USING HASH"
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
  "MAX_TIMER_MISC BIGINT unsigned not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (EVENT_NAME) USING HASH"
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
  "MAX_TIMER_MISC BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
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
  "LAST_ERROR_SEEN TIMESTAMP(0) null default 0,"
  "PRIMARY KEY (IP) USING HASH,"
  "KEY (HOST) USING HASH"
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
  "LOCKED_BY_THREAD_ID BIGINT unsigned,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (NAME) USING HASH,"
  "KEY (LOCKED_BY_THREAD_ID) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH"
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
-- TABLE DATA_LOCKS
--

SET @cmd="CREATE TABLE performance_schema.data_locks("
  "ENGINE VARCHAR(32) not null,"
  "ENGINE_LOCK_ID VARCHAR(128) not null,"
  "ENGINE_TRANSACTION_ID BIGINT unsigned,"
  "THREAD_ID BIGINT unsigned,"
  "EVENT_ID BIGINT unsigned,"
  "OBJECT_SCHEMA VARCHAR(64),"
  "OBJECT_NAME VARCHAR(64),"
  "PARTITION_NAME VARCHAR(64),"
  "SUBPARTITION_NAME VARCHAR(64),"
  "INDEX_NAME VARCHAR(64),"
  "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "LOCK_TYPE VARCHAR(32) not null,"
  "LOCK_MODE VARCHAR(32) not null,"
  "LOCK_STATUS VARCHAR(32) not null,"
  "LOCK_DATA VARCHAR(8192) CHARACTER SET utf8mb4,"
  "PRIMARY KEY (ENGINE_LOCK_ID, ENGINE) USING HASH,"
  "KEY (ENGINE_TRANSACTION_ID, ENGINE) USING HASH,"
  "KEY (THREAD_ID, EVENT_ID) USING HASH,"
  "KEY (OBJECT_SCHEMA, OBJECT_NAME, PARTITION_NAME, SUBPARTITION_NAME) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE DATA_LOCK_WAITS
--

SET @cmd="CREATE TABLE performance_schema.data_lock_waits("
  "ENGINE VARCHAR(32) not null,"
  "REQUESTING_ENGINE_LOCK_ID VARCHAR(128) not null,"
  "REQUESTING_ENGINE_TRANSACTION_ID BIGINT unsigned,"
  "REQUESTING_THREAD_ID BIGINT unsigned,"
  "REQUESTING_EVENT_ID BIGINT unsigned,"
  "REQUESTING_OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "BLOCKING_ENGINE_LOCK_ID VARCHAR(128) not null,"
  "BLOCKING_ENGINE_TRANSACTION_ID BIGINT unsigned,"
  "BLOCKING_THREAD_ID BIGINT unsigned,"
  "BLOCKING_EVENT_ID BIGINT unsigned,"
  "BLOCKING_OBJECT_INSTANCE_BEGIN BIGINT unsigned not null,"
  "KEY (REQUESTING_ENGINE_LOCK_ID, ENGINE) USING HASH,"
  "KEY (BLOCKING_ENGINE_LOCK_ID, ENGINE) USING HASH,"
  "KEY (REQUESTING_ENGINE_TRANSACTION_ID, ENGINE) USING HASH,"
  "KEY (BLOCKING_ENGINE_TRANSACTION_ID, ENGINE) USING HASH,"
  "KEY (REQUESTING_THREAD_ID, REQUESTING_EVENT_ID) USING HASH,"
  "KEY (BLOCKING_THREAD_ID, BLOCKING_EVENT_ID) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

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
  "READ_LOCKED_BY_COUNT INTEGER unsigned not null,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (NAME) USING HASH,"
  "KEY (WRITE_LOCKED_BY_THREAD_ID) USING HASH"
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
  "`ROLE` CHAR(32) collate utf8_bin default '%' not null,"
  "ENABLED ENUM ('YES', 'NO') not null default 'YES',"
  "HISTORY ENUM ('YES', 'NO') not null default 'YES',"
  "PRIMARY KEY (HOST, USER, `ROLE`) USING HASH"
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
  "ENABLED ENUM ('YES', 'NO') not null,"
  "PRIMARY KEY (NAME) USING HASH"
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
  "TIMED ENUM ('YES', 'NO') not null,"
  "PRIMARY KEY (NAME) USING HASH"
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
  "TIMED ENUM ('YES', 'NO') not null default 'YES',"
  "UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH"
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
  "TIMER_NAME ENUM ('CYCLE', 'NANOSECOND', 'MICROSECOND', 'MILLISECOND', 'TICK') not null,"
  "PRIMARY KEY (NAME) USING HASH"
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
  "MAX_TIMER_DELETE BIGINT unsigned not null,"
  "UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME, INDEX_NAME) USING HASH"
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
  "MAX_TIMER_DELETE BIGINT unsigned not null,"
  "UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH"
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
  "MAX_TIMER_WRITE_EXTERNAL BIGINT unsigned not null,"
  "UNIQUE KEY `OBJECT` (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH"
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
  "`ROLE` VARCHAR(64),"
  "INSTRUMENTED ENUM ('YES', 'NO') not null,"
  "HISTORY ENUM ('YES', 'NO') not null,"
  "CONNECTION_TYPE VARCHAR(16),"
  "THREAD_OS_ID BIGINT unsigned,"
  "PRIMARY KEY (THREAD_ID) USING HASH,"
  "KEY (PROCESSLIST_ID) USING HASH,"
  "KEY (THREAD_OS_ID) USING HASH,"
  "KEY (NAME) USING HASH,"
  "KEY `PROCESSLIST_ACCOUNT` (PROCESSLIST_USER, PROCESSLIST_HOST) USING HASH,"
  "KEY (PROCESSLIST_HOST) USING HASH"
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
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY (HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY (USER, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_WAIT BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
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
  "NESTING_EVENT_LEVEL INTEGER,"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "NESTING_EVENT_LEVEL INTEGER,"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "UNIQUE KEY (HOST, EVENT_NAME) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "UNIQUE KEY (USER, EVENT_NAME) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, EVENT_NAME) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
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
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "NESTING_EVENT_TYPE ENUM('TRANSACTION', 'STATEMENT', 'STAGE', 'WAIT'),"
  "PRIMARY KEY (THREAD_ID, EVENT_ID) USING HASH"
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
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null,"
  "PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null,"
  "UNIQUE KEY (HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null,"
  "UNIQUE KEY (USER, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, EVENT_NAME) USING HASH"
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
  "MAX_TIMER_READ_ONLY BIGINT unsigned not null,"
  "PRIMARY KEY (EVENT_NAME) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR
--

SET @cmd="CREATE TABLE performance_schema.events_errors_summary_by_account_by_error("
  "USER CHAR(32) collate utf8_bin default null,"
  "HOST CHAR(60) collate utf8_bin default null,"
  "ERROR_NUMBER INTEGER,"
  "ERROR_NAME VARCHAR(64),"
  "SQL_STATE VARCHAR(5),"
  "SUM_ERROR_RAISED  BIGINT unsigned not null,"
  "SUM_ERROR_HANDLED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) null default 0,"
  "LAST_SEEN TIMESTAMP(0) null default 0,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, ERROR_NUMBER) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

----
---- TABLE EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR
----

SET @cmd="CREATE TABLE performance_schema.events_errors_summary_by_host_by_error("
  "HOST CHAR(60) collate utf8_bin default null,"
  "ERROR_NUMBER INTEGER,"
  "ERROR_NAME VARCHAR(64),"
  "SQL_STATE VARCHAR(5),"
  "SUM_ERROR_RAISED  BIGINT unsigned not null,"
  "SUM_ERROR_HANDLED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) null default 0,"
  "LAST_SEEN TIMESTAMP(0) null default 0,"
  "UNIQUE KEY (HOST, ERROR_NUMBER) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

----
---- TABLE EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR
----

SET @cmd="CREATE TABLE performance_schema.events_errors_summary_by_user_by_error("
  "USER CHAR(32) collate utf8_bin default null,"
  "ERROR_NUMBER INTEGER,"
  "ERROR_NAME VARCHAR(64),"
  "SQL_STATE VARCHAR(5),"
  "SUM_ERROR_RAISED  BIGINT unsigned not null,"
  "SUM_ERROR_HANDLED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) null default 0,"
  "LAST_SEEN TIMESTAMP(0) null default 0,"
  "UNIQUE KEY (USER, ERROR_NUMBER) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR
--

SET @cmd="CREATE TABLE performance_schema.events_errors_summary_by_thread_by_error("
  "THREAD_ID BIGINT unsigned not null,"
  "ERROR_NUMBER INTEGER,"
  "ERROR_NAME VARCHAR(64),"
  "SQL_STATE VARCHAR(5),"
  "SUM_ERROR_RAISED  BIGINT unsigned not null,"
  "SUM_ERROR_HANDLED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) null default 0,"
  "LAST_SEEN TIMESTAMP(0) null default 0,"
  "UNIQUE KEY (THREAD_ID, ERROR_NUMBER) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_ERRORS_SUMMARY_GLOBAL_BY_ERROR
--

SET @cmd="CREATE TABLE performance_schema.events_errors_summary_global_by_error("
  "ERROR_NUMBER INTEGER,"
  "ERROR_NAME VARCHAR(64),"
  "SQL_STATE VARCHAR(5),"
  "SUM_ERROR_RAISED  BIGINT unsigned not null,"
  "SUM_ERROR_HANDLED BIGINT unsigned not null,"
  "FIRST_SEEN TIMESTAMP(0) null default 0,"
  "LAST_SEEN TIMESTAMP(0) null default 0,"
  "UNIQUE KEY (ERROR_NUMBER) USING HASH"
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
  "TOTAL_CONNECTIONS bigint not null,"
  "UNIQUE KEY (HOST) USING HASH"
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
  "TOTAL_CONNECTIONS bigint not null,"
  "UNIQUE KEY (USER) USING HASH"
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
  "TOTAL_CONNECTIONS bigint not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST) USING HASH"
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
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null,"
  "PRIMARY KEY (EVENT_NAME)"
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
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null,"
  "PRIMARY KEY (THREAD_ID, EVENT_NAME) USING HASH"
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
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null,"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, EVENT_NAME) USING HASH"
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
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null,"
  "UNIQUE KEY (HOST, EVENT_NAME) USING HASH"
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
  "HIGH_NUMBER_OF_BYTES_USED BIGINT not null,"
  "UNIQUE KEY (USER, EVENT_NAME) USING HASH"
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
  "LAST_SEEN TIMESTAMP(0) NOT NULL default 0,"
  "UNIQUE KEY (SCHEMA_NAME, DIGEST) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED bigint(20) unsigned NOT NULL,"
  "PRIMARY KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH"
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
  "SUM_NO_GOOD_INDEX_USED bigint(20) unsigned NOT NULL,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "UNIQUE KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH,"
  "KEY (STATEMENT_ID) USING HASH,"
  "KEY (STATEMENT_NAME) USING HASH,"
  "KEY (OWNER_OBJECT_TYPE, OWNER_OBJECT_SCHEMA, OWNER_OBJECT_NAME) USING HASH"
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
  "TLS_VERSION VARCHAR(255) not null,"
  "PRIMARY KEY (CHANNEL_NAME) USING HASH"
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
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null,"
  "PRIMARY KEY (CHANNEL_NAME) USING HASH,"
  "KEY (THREAD_ID) USING HASH"
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
  "DESIRED_DELAY INTEGER not null,"
  "PRIMARY KEY (CHANNEL_NAME) USING HASH"
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
  "COUNT_TRANSACTIONS_RETRIES BIGINT unsigned not null,"
  "PRIMARY KEY (CHANNEL_NAME) USING HASH"
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
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null,"
  "PRIMARY KEY (CHANNEL_NAME) USING HASH,"
  "KEY (THREAD_ID) USING HASH"
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
  "LAST_ERROR_TIMESTAMP TIMESTAMP(0) not null,"
  "PRIMARY KEY (CHANNEL_NAME, WORKER_ID) USING HASH,"
  "KEY (THREAD_ID) USING HASH"
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
  "ORDINAL_POSITION INT,"
  "PRIMARY KEY (PROCESSLIST_ID, ATTR_NAME)"
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
  "EXTERNAL_LOCK VARCHAR(64),"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH,"
  "KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH"
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
  "OWNER_EVENT_ID BIGINT unsigned,"
  "PRIMARY KEY (OBJECT_INSTANCE_BEGIN) USING HASH,"
  "KEY (OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME) USING HASH,"
  "KEY (OWNER_THREAD_ID, OWNER_EVENT_ID) USING HASH"
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
  "VARIABLE_VALUE LONGBLOB,"
  "PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (VARIABLE_NAME) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE VARIABLES_INFO
--

SET @cmd="CREATE TABLE performance_schema.variables_info("
  "VARIABLE_NAME varchar(64) not null,"
  "VARIABLE_SOURCE ENUM('COMPILED','GLOBAL','SERVER','EXPLICIT','EXTRA','USER','LOGIN','COMMAND_LINE','PERSISTED','DYNAMIC') DEFAULT 'COMPILED',"
  "VARIABLE_PATH varchar(1024),"
  "MIN_VALUE varchar(64),"
  "MAX_VALUE varchar(64),"
  "SET_TIME TIMESTAMP(0) NOT NULL DEFAULT CURRENT_TIMESTAMP,"
  "SET_USER CHAR(32) collate utf8_bin default null,"
  "SET_HOST CHAR(60) collate utf8_bin default null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE PERSISTED_VARIABLES
--

SET @cmd="CREATE TABLE performance_schema.persisted_variables("
  "VARIABLE_NAME VARCHAR(64) not null,"
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (THREAD_ID, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "UNIQUE KEY (USER, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "UNIQUE KEY (HOST, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "UNIQUE KEY `ACCOUNT` (USER, HOST, VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (VARIABLE_NAME) USING HASH"
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
  "VARIABLE_VALUE VARCHAR(1024),"
  "PRIMARY KEY (VARIABLE_NAME) USING HASH"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

CREATE TABLE IF NOT EXISTS proxies_priv (Host char(60) binary DEFAULT '' NOT NULL, User char(32) binary DEFAULT '' NOT NULL, Proxied_host char(60) binary DEFAULT '' NOT NULL, Proxied_user char(32) binary DEFAULT '' NOT NULL, With_grant BOOL DEFAULT 0 NOT NULL, Grantor char(93) DEFAULT '' NOT NULL, Timestamp timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, PRIMARY KEY Host (Host,User,Proxied_host,Proxied_user), KEY Grantor (Grantor) ) engine=InnoDB STATS_PERSISTENT=0 CHARACTER SET utf8 COLLATE utf8_bin comment='User proxy privileges';

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
  PRIMARY KEY(epoch, orig_server_id, orig_epoch)) ENGINE=INNODB STATS_PERSISTENT=0";

SET @str = IF(@have_ndb = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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

# Recreate lookup tables in ndbinfo
# ndbinfo.ndb$acc_operations
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$acc_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$acc_operations` (`node_id` INT UNSIGNED COMMENT "node_id",`block_instance` INT UNSIGNED COMMENT "Block instance",`tableid` INT UNSIGNED COMMENT "Table id",`fragmentid` INT UNSIGNED COMMENT "Fragment id",`rowid` BIGINT UNSIGNED COMMENT "Row id in fragment",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`acc_op_id` INT UNSIGNED COMMENT "Operation id",`op_flags` INT UNSIGNED COMMENT "Operation flags",`prev_serial_op_id` INT UNSIGNED COMMENT "Prev serial op id",`next_serial_op_id` INT UNSIGNED COMMENT "Next serial op id",`prev_parallel_op_id` INT UNSIGNED COMMENT "Prev parallel op id",`next_parallel_op_id` INT UNSIGNED COMMENT "Next parallel op id",`duration_millis` INT UNSIGNED COMMENT "Duration of wait/hold",`user_ptr` INT UNSIGNED COMMENT "Lock requestor context") COMMENT="ACC operation info" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$config_values
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_values`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_values` (`node_id` INT UNSIGNED,`config_param` INT UNSIGNED COMMENT "Parameter number",`config_value` VARCHAR(512) COMMENT "Parameter value") COMMENT="Configuration parameter values" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$cpustat
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user` INT UNSIGNED COMMENT "Percentage time spent in user mode as reported by OS",`OS_system` INT UNSIGNED COMMENT "Percentage time spent in system mode as reported by OS",`OS_idle` INT UNSIGNED COMMENT "Percentage time spent in idle mode as reported by OS",`thread_exec` INT UNSIGNED COMMENT "Percentage time spent executing as calculated by thread",`thread_sleeping` INT UNSIGNED COMMENT "Percentage time spent sleeping as calculated by thread",`thread_spinning` INT UNSIGNED COMMENT "Percentage time spent spinning as calculated by thread",`thread_send` INT UNSIGNED COMMENT "Percentage time spent sending as calculated by thread",`thread_buffer_full` INT UNSIGNED COMMENT "Percentage time spent in buffer full as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats for last second" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_1sec
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_1sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_1sec` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 1 second intervals" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_20sec
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_20sec`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_20sec` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 20 seconds intervals" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$cpustat_50ms
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$cpustat_50ms`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$cpustat_50ms` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`OS_user_time` INT UNSIGNED COMMENT "User time in microseconds as reported by OS",`OS_system_time` INT UNSIGNED COMMENT "System time in microseconds as reported by OS",`OS_idle_time` INT UNSIGNED COMMENT "Idle time in microseconds as reported by OS",`exec_time` INT UNSIGNED COMMENT "Execution time in microseconds as calculated by thread",`sleep_time` INT UNSIGNED COMMENT "Sleep time in microseconds as calculated by thread",`spin_time` INT UNSIGNED COMMENT "Spin time in microseconds as calculated by thread",`send_time` INT UNSIGNED COMMENT "Send time in microseconds as calculated by thread",`buffer_full_time` INT UNSIGNED COMMENT "Time spent with buffer full in microseconds as calculated by thread",`elapsed_time` INT UNSIGNED COMMENT "Elapsed time in microseconds for measurement") COMMENT="Thread CPU stats at 50 milliseconds intervals" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$disk_write_speed_aggregate
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_aggregate`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_aggregate` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`backup_lcp_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP last second",`redo_speed_last_sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log last second",`backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 10 seconds",`redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 10 seconds",`std_dev_backup_lcp_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 10 seconds",`std_dev_redo_speed_last_10sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 10 seconds",`backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written by backup and LCP per second last 60 seconds",`redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Number of bytes written to REDO log per second last 60 seconds",`std_dev_backup_lcp_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written by backup and LCP per second last 60 seconds",`std_dev_redo_speed_last_60sec` BIGINT UNSIGNED COMMENT "Standard deviation of Number of bytes written to REDO log per second last 60 seconds",`slowdowns_due_to_io_lag` BIGINT UNSIGNED COMMENT "Number of seconds that we slowed down disk writes due to REDO log IO lagging",`slowdowns_due_to_high_cpu` BIGINT UNSIGNED COMMENT "Number of seconds we slowed down disk writes due to high CPU usage of LDM thread",`disk_write_speed_set_to_min` BIGINT UNSIGNED COMMENT "Number of seconds we set disk write speed to a minimum",`current_target_disk_write_speed` BIGINT UNSIGNED COMMENT "Current target of disk write speed in bytes per second") COMMENT="Actual speed of disk writes per LDM thread, aggregate data" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$disk_write_speed_base
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$disk_write_speed_base`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$disk_write_speed_base` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "LDM thread instance",`millis_ago` BIGINT UNSIGNED COMMENT "Milliseconds ago since this period finished",`millis_passed` BIGINT UNSIGNED COMMENT "Milliseconds passed in the period reported",`backup_lcp_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written by backup and LCP in the period",`redo_bytes_written` BIGINT UNSIGNED COMMENT "Bytes written to REDO log in the period",`target_disk_write_speed` BIGINT UNSIGNED COMMENT "Target disk write speed in bytes per second at the measurement point") COMMENT="Actual speed of disk writes per LDM thread, base data" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$frag_locks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_locks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_locks` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`ex_req` BIGINT UNSIGNED COMMENT "Exclusive row lock request count",`ex_imm_ok` BIGINT UNSIGNED COMMENT "Exclusive row lock immediate grants",`ex_wait_ok` BIGINT UNSIGNED COMMENT "Exclusive row lock grants with wait",`ex_wait_fail` BIGINT UNSIGNED COMMENT "Exclusive row lock failed grants",`sh_req` BIGINT UNSIGNED COMMENT "Shared row lock request count",`sh_imm_ok` BIGINT UNSIGNED COMMENT "Shared row lock immediate grants",`sh_wait_ok` BIGINT UNSIGNED COMMENT "Shared row lock grants with wait",`sh_wait_fail` BIGINT UNSIGNED COMMENT "Shared row lock failed grants",`wait_ok_millis` BIGINT UNSIGNED COMMENT "Time spent waiting before successfully claiming a lock",`wait_fail_millis` BIGINT UNSIGNED COMMENT "Time spent waiting before failing to claim a lock") COMMENT="Per fragment lock information" ENGINE=NDBINFO','SET @dummy = 0');
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
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$frag_operations`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$frag_operations` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "LQH instance no",`table_id` INT UNSIGNED COMMENT "Table identity",`fragment_num` INT UNSIGNED COMMENT "Fragment number",`tot_key_reads` BIGINT UNSIGNED COMMENT "Total number of key reads received",`tot_key_inserts` BIGINT UNSIGNED COMMENT "Total number of key inserts received",`tot_key_updates` BIGINT UNSIGNED COMMENT "Total number of key updates received",`tot_key_writes` BIGINT UNSIGNED COMMENT "Total number of key writes received",`tot_key_deletes` BIGINT UNSIGNED COMMENT "Total number of key deletes received",`tot_key_refs` BIGINT UNSIGNED COMMENT "Total number of key operations refused by LDM",`tot_key_attrinfo_bytes` BIGINT UNSIGNED COMMENT "Total attrinfo bytes received for key operations",`tot_key_keyinfo_bytes` BIGINT UNSIGNED COMMENT "Total keyinfo bytes received for key operations",`tot_key_prog_bytes` BIGINT UNSIGNED COMMENT "Total bytes of filter programs for key operations",`tot_key_inst_exec` BIGINT UNSIGNED COMMENT "Total number of interpreter instructions executed for key operations",`tot_key_bytes_returned` BIGINT UNSIGNED COMMENT "Total number of bytes returned to client for key operations",`tot_frag_scans` BIGINT UNSIGNED COMMENT "Total number of fragment scans received",`tot_scan_rows_examined` BIGINT UNSIGNED COMMENT "Total number of rows examined by scans",`tot_scan_rows_returned` BIGINT UNSIGNED COMMENT "Total number of rows returned to client by scan",`tot_scan_bytes_returned` BIGINT UNSIGNED COMMENT "Total number of bytes returned to client by scans",`tot_scan_prog_bytes` BIGINT UNSIGNED COMMENT "Total bytes of scan filter programs",`tot_scan_bound_bytes` BIGINT UNSIGNED COMMENT "Total bytes of scan bounds",`tot_scan_inst_exec` BIGINT UNSIGNED COMMENT "Total number of interpreter instructions executed for scans",`tot_qd_frag_scans` BIGINT UNSIGNED COMMENT "Total number of fragment scans queued before exec",`conc_frag_scans` INT UNSIGNED COMMENT "Number of frag scans currently running",`conc_qd_plain_frag_scans` INT UNSIGNED COMMENT "Number of tux frag scans currently queued",`conc_qd_tup_frag_scans` INT UNSIGNED COMMENT "Number of tup frag scans currently queued",`conc_qd_acc_frag_scans` INT UNSIGNED COMMENT "Number of acc frag scans currently queued",`tot_commits` BIGINT UNSIGNED COMMENT "Total number of committed row changes") COMMENT="Per fragment operational information" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$logspaces
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$logspaces`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$logspaces` (`node_id` INT UNSIGNED,`log_type` INT UNSIGNED COMMENT "0 = REDO, 1 = DD-UNDO",`log_id` INT UNSIGNED,`log_part` INT UNSIGNED,`total` BIGINT UNSIGNED COMMENT "total allocated",`used` BIGINT UNSIGNED COMMENT "currently in use",`high` BIGINT UNSIGNED COMMENT "in use high water mark") COMMENT="logspace usage" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$nodes
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$nodes`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$nodes` (`node_id` INT UNSIGNED,`uptime` BIGINT UNSIGNED COMMENT "time in seconds that node has been running",`status` INT UNSIGNED COMMENT "starting/started/stopped etc.",`start_phase` INT UNSIGNED COMMENT "start phase if node is starting",`config_generation` INT UNSIGNED COMMENT "configuration generation number") COMMENT="node status" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$pools
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$pools`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$pools` (`node_id` INT UNSIGNED,`block_number` INT UNSIGNED,`block_instance` INT UNSIGNED,`pool_name` VARCHAR(512),`used` BIGINT UNSIGNED COMMENT "currently in use",`total` BIGINT UNSIGNED COMMENT "total allocated",`high` BIGINT UNSIGNED COMMENT "in use high water mark",`entry_size` BIGINT UNSIGNED COMMENT "size in bytes of each object",`config_param1` INT UNSIGNED COMMENT "config param 1 affecting pool",`config_param2` INT UNSIGNED COMMENT "config param 2 affecting pool",`config_param3` INT UNSIGNED COMMENT "config param 3 affecting pool",`config_param4` INT UNSIGNED COMMENT "config param 4 affecting pool") COMMENT="pool usage" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$restart_info
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$restart_info`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$restart_info` (`node_id` INT UNSIGNED COMMENT "node id",`node_restart_status` VARCHAR(512) COMMENT "Current state of node recovery",`node_restart_status_int` INT UNSIGNED COMMENT "Current state of node recovery as number",`secs_to_complete_node_failure` INT UNSIGNED COMMENT "Seconds to complete node failure handling",`secs_to_allocate_node_id` INT UNSIGNED COMMENT "Seconds from node failure completion to allocation of node id",`secs_to_include_in_heartbeat_protocol` INT UNSIGNED COMMENT "Seconds from allocation of node id to inclusion in HB protocol",`secs_until_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds from included in HB protocol until we wait for ndbcntr master",`secs_wait_for_ndbcntr_master` INT UNSIGNED COMMENT "Seconds we waited for being accepted by NDBCNTR master to start",`secs_to_get_start_permitted` INT UNSIGNED COMMENT "Seconds from permit by master until all nodes accepted our start",`secs_to_wait_for_lcp_for_copy_meta_data` INT UNSIGNED COMMENT "Seconds waiting for LCP completion before copying meta data",`secs_to_copy_meta_data` INT UNSIGNED COMMENT "Seconds to copy meta data to starting node from master",`secs_to_include_node` INT UNSIGNED COMMENT "Seconds to wait for GCP and inclusion of all nodes into protocols",`secs_starting_node_to_request_local_recovery` INT UNSIGNED COMMENT "Seconds for starting node to request local recovery",`secs_for_local_recovery` INT UNSIGNED COMMENT "Seconds for local recovery in starting node",`secs_restore_fragments` INT UNSIGNED COMMENT "Seconds to restore fragments from LCP files",`secs_undo_disk_data` INT UNSIGNED COMMENT "Seconds to execute UNDO log on disk data part of records",`secs_exec_redo_log` INT UNSIGNED COMMENT "Seconds to execute REDO log on all restored fragments",`secs_index_rebuild` INT UNSIGNED COMMENT "Seconds to rebuild indexes on restored fragments",`secs_to_synchronize_starting_node` INT UNSIGNED COMMENT "Seconds to synchronize starting node from live nodes",`secs_wait_lcp_for_restart` INT UNSIGNED COMMENT "Seconds to wait for LCP start and completion before restart is completed",`secs_wait_subscription_handover` INT UNSIGNED COMMENT "Seconds waiting for handover of replication subscriptions",`total_restart_secs` INT UNSIGNED COMMENT "Total number of seconds from node failure until node is started again") COMMENT="Times of restart phases in seconds and current state" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$stored_tables
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$stored_tables`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$stored_tables` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`logged_table` INT UNSIGNED COMMENT "Is table logged",`row_contains_gci` INT UNSIGNED COMMENT "Does table rows contains GCI",`row_contains_checksum` INT UNSIGNED COMMENT "Does table rows contain checksum",`temporary_table` INT UNSIGNED COMMENT "Is table temporary",`force_var_part` INT UNSIGNED COMMENT "Force var part active",`read_backup` INT UNSIGNED COMMENT "Is backup replicas read",`fully_replicated` INT UNSIGNED COMMENT "Is table fully replicated",`extra_row_gci` INT UNSIGNED COMMENT "extra_row_gci",`extra_row_author` INT UNSIGNED COMMENT "extra_row_author",`storage_type` INT UNSIGNED COMMENT "Storage type of table",`hashmap_id` INT UNSIGNED COMMENT "Hashmap id",`hashmap_version` INT UNSIGNED COMMENT "Hashmap version",`table_version` INT UNSIGNED COMMENT "Table version",`fragment_type` INT UNSIGNED COMMENT "Type of fragmentation",`partition_balance` INT UNSIGNED COMMENT "Partition balance",`create_gci` INT UNSIGNED COMMENT "GCI in which table was created",`backup_locked` INT UNSIGNED COMMENT "Locked for backup",`single_user_mode` INT UNSIGNED COMMENT "Is single user mode active") COMMENT="Information about stored tables" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_distribution_status
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_distribution_status`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_distribution_status` (`node_id` INT UNSIGNED COMMENT "Node id",`table_id` INT UNSIGNED COMMENT "Table id",`tab_copy_status` INT UNSIGNED COMMENT "Copy status of the table",`tab_update_status` INT UNSIGNED COMMENT "Update status of the table",`tab_lcp_status` INT UNSIGNED COMMENT "LCP status of the table",`tab_status` INT UNSIGNED COMMENT "Create status of the table",`tab_storage` INT UNSIGNED COMMENT "Storage type of table",`tab_type` INT UNSIGNED COMMENT "Type of table",`tab_partitions` INT UNSIGNED COMMENT "Number of partitions in table",`tab_fragments` INT UNSIGNED COMMENT "Number of fragments in table",`current_scan_count` INT UNSIGNED COMMENT "Current number of active scans",`scan_count_wait` INT UNSIGNED COMMENT "Number of scans waiting for",`is_reorg_ongoing` INT UNSIGNED COMMENT "Is a table reorg ongoing on table") COMMENT="Table status in distribution handler" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_distribution_status_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_distribution_status_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_distribution_status_all` (`node_id` INT UNSIGNED COMMENT "Node id",`table_id` INT UNSIGNED COMMENT "Table id",`tab_copy_status` INT UNSIGNED COMMENT "Copy status of the table",`tab_update_status` INT UNSIGNED COMMENT "Update status of the table",`tab_lcp_status` INT UNSIGNED COMMENT "LCP status of the table",`tab_status` INT UNSIGNED COMMENT "Create status of the table",`tab_storage` INT UNSIGNED COMMENT "Storage type of table",`tab_type` INT UNSIGNED COMMENT "Type of table",`tab_partitions` INT UNSIGNED COMMENT "Number of partitions in table",`tab_fragments` INT UNSIGNED COMMENT "Number of fragments in table",`current_scan_count` INT UNSIGNED COMMENT "Current number of active scans",`scan_count_wait` INT UNSIGNED COMMENT "Number of scans waiting for",`is_reorg_ongoing` INT UNSIGNED COMMENT "Is a table reorg ongoing on table") COMMENT="Table status in distribution handler" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_fragments
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_fragments`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_fragments` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`partition_id` INT UNSIGNED COMMENT "Partition id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`partition_order` INT UNSIGNED COMMENT "Order of fragment in partition",`log_part_id` INT UNSIGNED COMMENT "Log part id of fragment",`no_of_replicas` INT UNSIGNED COMMENT "Number of replicas",`current_primary` INT UNSIGNED COMMENT "Current primary node id",`preferred_primary` INT UNSIGNED COMMENT "Preferred primary node id",`current_first_backup` INT UNSIGNED COMMENT "Current first backup node id",`current_second_backup` INT UNSIGNED COMMENT "Current second backup node id",`current_third_backup` INT UNSIGNED COMMENT "Current third backup node id",`num_alive_replicas` INT UNSIGNED COMMENT "Current number of alive replicas",`num_dead_replicas` INT UNSIGNED COMMENT "Current number of dead replicas",`num_lcp_replicas` INT UNSIGNED COMMENT "Number of replicas remaining to be LCP:ed") COMMENT="Partitions of the tables" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_fragments_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_fragments_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_fragments_all` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`partition_id` INT UNSIGNED COMMENT "Partition id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`partition_order` INT UNSIGNED COMMENT "Order of fragment in partition",`log_part_id` INT UNSIGNED COMMENT "Log part id of fragment",`no_of_replicas` INT UNSIGNED COMMENT "Number of replicas",`current_primary` INT UNSIGNED COMMENT "Current primary node id",`preferred_primary` INT UNSIGNED COMMENT "Preferred primary node id",`current_first_backup` INT UNSIGNED COMMENT "Current first backup node id",`current_second_backup` INT UNSIGNED COMMENT "Current second backup node id",`current_third_backup` INT UNSIGNED COMMENT "Current third backup node id",`num_alive_replicas` INT UNSIGNED COMMENT "Current number of alive replicas",`num_dead_replicas` INT UNSIGNED COMMENT "Current number of dead replicas",`num_lcp_replicas` INT UNSIGNED COMMENT "Number of replicas remaining to be LCP:ed") COMMENT="Partitions of the tables" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_replicas
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_replicas`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_replicas` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`initial_gci` INT UNSIGNED COMMENT "Initial GCI for table",`replica_node_id` INT UNSIGNED COMMENT "Node id where replica is stored",`is_lcp_ongoing` INT UNSIGNED COMMENT "Is LCP ongoing on this fragment",`num_crashed_replicas` INT UNSIGNED COMMENT "Number of crashed replica instances",`last_max_gci_started` INT UNSIGNED COMMENT "Last LCP Max GCI started",`last_max_gci_completed` INT UNSIGNED COMMENT "Last LCP Max GCI completed",`last_lcp_id` INT UNSIGNED COMMENT "Last LCP id",`prev_lcp_id` INT UNSIGNED COMMENT "Previous LCP id",`prev_max_gci_started` INT UNSIGNED COMMENT "Previous LCP Max GCI started",`prev_max_gci_completed` INT UNSIGNED COMMENT "Previous LCP Max GCI completed",`last_create_gci` INT UNSIGNED COMMENT "Last Create GCI of last crashed replica instance",`last_replica_gci` INT UNSIGNED COMMENT "Last GCI of last crashed replica instance",`is_replica_alive` INT UNSIGNED COMMENT "Is replica alive or not") COMMENT="Fragment replicas of the tables" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$table_replicas_all
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$table_replicas_all`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$table_replicas_all` (`node_id` INT UNSIGNED COMMENT "node_id",`table_id` INT UNSIGNED COMMENT "Table id",`fragment_id` INT UNSIGNED COMMENT "Fragment id",`initial_gci` INT UNSIGNED COMMENT "Initial GCI for table",`replica_node_id` INT UNSIGNED COMMENT "Node id where replica is stored",`is_lcp_ongoing` INT UNSIGNED COMMENT "Is LCP ongoing on this fragment",`num_crashed_replicas` INT UNSIGNED COMMENT "Number of crashed replica instances",`last_max_gci_started` INT UNSIGNED COMMENT "Last LCP Max GCI started",`last_max_gci_completed` INT UNSIGNED COMMENT "Last LCP Max GCI completed",`last_lcp_id` INT UNSIGNED COMMENT "Last LCP id",`prev_lcp_id` INT UNSIGNED COMMENT "Previous LCP id",`prev_max_gci_started` INT UNSIGNED COMMENT "Previous LCP Max GCI started",`prev_max_gci_completed` INT UNSIGNED COMMENT "Previous LCP Max GCI completed",`last_create_gci` INT UNSIGNED COMMENT "Last Create GCI of last crashed replica instance",`last_replica_gci` INT UNSIGNED COMMENT "Last GCI of last crashed replica instance",`is_replica_alive` INT UNSIGNED COMMENT "Is replica alive or not") COMMENT="Fragment replicas of the tables" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$tc_time_track_stats
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$tc_time_track_stats`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$tc_time_track_stats` (`node_id` INT UNSIGNED COMMENT "node id",`block_number` INT UNSIGNED COMMENT "Block number",`block_instance` INT UNSIGNED COMMENT "Block instance",`comm_node_id` INT UNSIGNED COMMENT "node_id of API or DB",`upper_bound` BIGINT UNSIGNED COMMENT "Upper bound in micros of interval",`scans` BIGINT UNSIGNED COMMENT "scan histogram interval",`scan_errors` BIGINT UNSIGNED COMMENT "scan error histogram interval",`scan_fragments` BIGINT UNSIGNED COMMENT "scan fragment histogram interval",`scan_fragment_errors` BIGINT UNSIGNED COMMENT "scan fragment error histogram interval",`transactions` BIGINT UNSIGNED COMMENT "transaction histogram interval",`transaction_errors` BIGINT UNSIGNED COMMENT "transaction error histogram interval",`read_key_ops` BIGINT UNSIGNED COMMENT "read key operation histogram interval",`write_key_ops` BIGINT UNSIGNED COMMENT "write key operation histogram interval",`index_key_ops` BIGINT UNSIGNED COMMENT "index key operation histogram interval",`key_op_errors` BIGINT UNSIGNED COMMENT "key operation error histogram interval") COMMENT="Time tracking of transaction, key operations and scan ops" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$threadblocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threadblocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threadblocks` (`node_id` INT UNSIGNED COMMENT "node id",`thr_no` INT UNSIGNED COMMENT "thread number",`block_number` INT UNSIGNED COMMENT "block number",`block_instance` INT UNSIGNED COMMENT "block instance") COMMENT="which blocks are run in which threads" ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$threads
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$threads`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$threads` (`node_id` INT UNSIGNED COMMENT "node_id",`thr_no` INT UNSIGNED COMMENT "thread number",`thread_name` VARCHAR(512) COMMENT "thread_name",`thread_description` VARCHAR(512) COMMENT "thread_description") COMMENT="Base table for threads" ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$transactions
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$transactions`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$transactions` (`node_id` INT UNSIGNED COMMENT "node id",`block_instance` INT UNSIGNED COMMENT "TC instance no",`objid` INT UNSIGNED COMMENT "Object id of transaction object",`apiref` INT UNSIGNED COMMENT "API reference",`transid0` INT UNSIGNED COMMENT "Transaction id",`transid1` INT UNSIGNED COMMENT "Transaction id",`state` INT UNSIGNED COMMENT "Transaction state",`flags` INT UNSIGNED COMMENT "Transaction flags",`c_ops` INT UNSIGNED COMMENT "No of operations in transaction",`outstanding` INT UNSIGNED COMMENT "Currently outstanding request",`timer` INT UNSIGNED COMMENT "Timer (seconds)") COMMENT="transactions" ENGINE=NDBINFO','SET @dummy = 0');
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

# Recreate handler local lookup tables in ndbinfo
# ndbinfo.ndb$blocks
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$blocks`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$blocks` (block_number INT UNSIGNED, block_name VARCHAR(512)) ENGINE=NDBINFO','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.ndb$config_params
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$config_params`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$config_params` (param_number INT UNSIGNED, param_name VARCHAR(512), param_description VARCHAR(512), param_type VARCHAR(512), param_default VARCHAR(512), param_min VARCHAR(512), param_max VARCHAR(512), param_mandatory INT UNSIGNED, param_status VARCHAR(512)) ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.ndb$dbtc_apiconnect_state
SET @str=IF(@have_ndbinfo,'DROP TABLE IF EXISTS `ndbinfo`.`ndb$dbtc_apiconnect_state`','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @str=IF(@have_ndbinfo,'CREATE TABLE `ndbinfo`.`ndb$dbtc_apiconnect_state` (state_int_value INT UNSIGNED, state_name VARCHAR(256), state_friendly_name VARCHAR(256), state_description VARCHAR(256)) ENGINE=NDBINFO','SET @dummy = 0');
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

# ndbinfo.locks_per_fragment
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`locks_per_fragment` AS SELECT name.fq_name, parent_name.fq_name AS parent_fq_name, types.type_name AS type, table_id, node_id, block_instance, fragment_num, ex_req, ex_imm_ok, ex_wait_ok, ex_wait_fail, sh_req, sh_imm_ok, sh_wait_ok, sh_wait_fail, wait_ok_millis, wait_fail_millis FROM `ndbinfo`.`ndb$frag_locks` AS locks JOIN `ndbinfo`.`ndb$dict_obj_info` AS name ON name.id=locks.table_id AND name.type<=6 JOIN `ndbinfo`.`ndb$dict_obj_types` AS types ON name.type=types.type_id LEFT JOIN `ndbinfo`.`ndb$dict_obj_info` AS parent_name ON name.parent_obj_id=parent_name.id AND name.parent_obj_type=parent_name.type','SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

# ndbinfo.logbuffers
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`logbuffers` AS SELECT node_id,  CASE log_type  WHEN 0 THEN "REDO"  WHEN 1 THEN "DD-UNDO"  ELSE "<unknown>"  END AS log_type, log_id, log_part, total, used FROM `ndbinfo`.`ndb$logbuffers`','SET @dummy = 0');
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
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`memoryusage` AS SELECT node_id,  pool_name AS memory_type,  SUM(used*entry_size) AS used,  SUM(used) AS used_pages,  SUM(total*entry_size) AS total,  SUM(total) AS total_pages FROM `ndbinfo`.`ndb$pools` WHERE ( block_number IN (248, 254) AND   (pool_name = "Index memory" OR pool_name = "Data memory") ) OR pool_name = "Long message buffer" GROUP BY node_id, memory_type','SET @dummy = 0');
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

# ndbinfo.resources
SET @str=IF(@have_ndbinfo,'CREATE OR REPLACE DEFINER=`root`@`localhost` SQL SECURITY INVOKER VIEW `ndbinfo`.`resources` AS SELECT node_id,  CASE resource_id  WHEN 0 THEN "RESERVED"  WHEN 1 THEN "DISK_OPERATIONS"  WHEN 2 THEN "DISK_RECORDS"  WHEN 3 THEN "DATA_MEMORY"  WHEN 4 THEN "JOBBUFFER"  WHEN 5 THEN "FILE_BUFFERS"  WHEN 6 THEN "TRANSPORTER_BUFFERS"  WHEN 7 THEN "DISK_PAGE_BUFFER"  WHEN 8 THEN "QUERY_MEMORY"  WHEN 9 THEN "SCHEMA_TRANS_MEMORY"  ELSE "<unknown>"  END AS resource_name, reserved, used, max FROM `ndbinfo`.`ndb$resources`','SET @dummy = 0');
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

--
-- INFORMATION SCHEMA VIEWS implementing SHOW statements
--

CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.SHOW_STATISTICS AS
  (SELECT
    TABLE_SCHEMA as `Database`,
    sb.TABLE_NAME AS `Table`,
    NON_UNIQUE AS `Non_unique`,
    sb.INDEX_NAME AS `Key_name`,
    SEQ_IN_INDEX AS `Seq_in_index`,
    sb.COLUMN_NAME AS `Column_name`,
    COLLATION AS `Collation`,
    stat.cardinality AS `Cardinality`,
    SUB_PART AS `Sub_part`,
    PACKED AS `Packed`,
    NULLABLE AS `Null`,
    INDEX_TYPE AS `Index_type`,
    COMMENT AS `Comment`,
    INDEX_COMMENT AS `Index_comment`,
    IS_VISIBLE AS `Visible`,
    INDEX_ORDINAL_POSITION,
    COLUMN_ORDINAL_POSITION
  FROM information_schema.STATISTICS_BASE sb
    LEFT JOIN mysql.index_stats stat
                 ON sb.table_name=stat.table_name
                and sb.table_schema=stat.schema_name
                and sb.index_name=stat.index_name
                and sb.column_name=stat.column_name);


CREATE OR REPLACE DEFINER=`root`@`localhost` VIEW information_schema.SHOW_STATISTICS_DYNAMIC AS
  (SELECT
    TABLE_SCHEMA as `Database`,
    TABLE_NAME AS `Table`,
    NON_UNIQUE AS `Non_unique`,
    INDEX_NAME AS `Key_name`,
    SEQ_IN_INDEX AS `Seq_in_index`,
    COLUMN_NAME AS `Column_name`,
    COLLATION AS `Collation`,
    INTERNAL_INDEX_COLUMN_CARDINALITY(TABLE_SCHEMA, TABLE_NAME, INDEX_NAME,
                                      INDEX_ORDINAL_POSITION,
                                      COLUMN_ORDINAL_POSITION,
                                      ENGINE, SE_PRIVATE_ID, IS_HIDDEN)
      AS `Cardinality`,
    SUB_PART AS `Sub_part`,
    PACKED AS `Packed`,
    NULLABLE AS `Null`,
    INDEX_TYPE AS `Index_type`,
    COMMENT AS `Comment`,
    INDEX_COMMENT AS `Index_comment`,
    IS_VISIBLE AS `Visible`,
    INDEX_ORDINAL_POSITION,
    COLUMN_ORDINAL_POSITION
  FROM information_schema.STATISTICS_BASE);

