CREATE DATABASE mysql;
CREATE DATABASE test;

USE mysql;

CREATE TABLE db (Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL, References_priv enum('N','Y') DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,Db,User), KEY User (User)) comment='Database privileges';

INSERT INTO db VALUES ('%','test','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y');
INSERT INTO db VALUES ('%','test\_%','','Y','Y','Y','Y','Y','Y','N','Y','Y','Y','Y','Y');
 
CREATE TABLE host (Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL, References_priv enum('N','Y') DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL, PRIMARY KEY Host (Host,Db)) comment='Host privileges;  Merged with database privileges';

CREATE TABLE user (Host char(60) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Password char(41) binary DEFAULT '' NOT NULL, Select_priv enum('N','Y') DEFAULT 'N' NOT NULL, Insert_priv enum('N','Y') DEFAULT 'N' NOT NULL, Update_priv enum('N','Y') DEFAULT 'N' NOT NULL, Delete_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_priv enum('N','Y') DEFAULT 'N' NOT NULL, Drop_priv enum('N','Y') DEFAULT 'N' NOT NULL, Reload_priv enum('N','Y') DEFAULT 'N' NOT NULL, Shutdown_priv enum('N','Y') DEFAULT 'N' NOT NULL, Process_priv enum('N','Y') DEFAULT 'N' NOT NULL, File_priv enum('N','Y') DEFAULT 'N' NOT NULL, Grant_priv enum('N','Y') DEFAULT 'N' NOT NULL, References_priv enum('N','Y') DEFAULT 'N' NOT NULL, Index_priv enum('N','Y') DEFAULT 'N' NOT NULL, Alter_priv enum('N','Y') DEFAULT 'N' NOT NULL, Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL, Super_priv enum('N','Y') DEFAULT 'N' NOT NULL, Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL, Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL, Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL, Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL, Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL, ssl_type enum('','ANY','X509', 'SPECIFIED') DEFAULT '' NOT NULL, ssl_cipher BLOB NOT NULL, x509_issuer BLOB NOT NULL, x509_subject BLOB NOT NULL, max_questions int(11) unsigned DEFAULT 0 NOT NULL, max_updates int(11) unsigned DEFAULT 0 NOT NULL, max_connections int(11) unsigned DEFAULT 0 NOT NULL, PRIMARY KEY Host (Host,User)) comment='Users and global privileges';

INSERT INTO user VALUES ('localhost','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);
INSERT INTO user VALUES ('','root','','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','Y','','','','',0,0,0);

INSERT INTO user (host,user) values ('localhost','');
INSERT INTO user (host,user) values ('','');

CREATE TABLE func (name char(64) binary DEFAULT '' NOT NULL, ret tinyint(1) DEFAULT '0' NOT NULL, dl char(128) DEFAULT '' NOT NULL, type enum ('function','aggregate') NOT NULL, PRIMARY KEY (name)) comment='User defined functions';

CREATE TABLE tables_priv (Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Grantor char(77) DEFAULT '' NOT NULL, Timestamp timestamp(14), Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter') DEFAULT '' NOT NULL, Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name), KEY Grantor (Grantor)) comment='Table privileges';

CREATE TABLE columns_priv (Host char(60) binary DEFAULT '' NOT NULL, Db char(64) binary DEFAULT '' NOT NULL, User char(16) binary DEFAULT '' NOT NULL, Table_name char(64) binary DEFAULT '' NOT NULL, Column_name char(64) binary DEFAULT '' NOT NULL, Timestamp timestamp(14), Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL, PRIMARY KEY (Host,Db,User,Table_name,Column_name)) comment='Column privileges';

CREATE TABLE help_topic (help_topic_id int unsigned NOT NULL, name varchar(64) NOT NULL, help_category_id smallint unsigned NOT NULL, description text NOT NULL, example text NOT NULL, url varchar(128) NOT NULL, primary key (help_topic_id), unique index (name))comment='help topics';
CREATE TABLE help_category (help_category_id smallint unsigned NOT NULL, name varchar(64) NOT NULL, parent_category_id smallint unsigned null, url varchar(128) NOT NULL, primary key (help_category_id), unique index (name)) comment='help categories';
CREATE TABLE help_keyword (help_keyword_id int unsigned NOT NULL, name varchar(64) NOT NULL, primary key  (help_keyword_id), unique index (name)) comment='help keywords';
CREATE TABLE help_relation (help_topic_id int unsigned NOT NULL references help_topic, help_keyword_id  int unsigned NOT NULL references help_keyword, primary key (help_keyword_id, help_topic_id)) comment='keyword-topic relation';

CREATE TABLE time_zone_name (Name char(64) NOT NULL,Time_zone_id int unsigned NOT NULL,PRIMARY KEY Name (Name)) DEFAULT CHARACTER SET latin1 comment='Time zone names';

CREATE TABLE time_zone (Time_zone_id int unsigned NOT NULL auto_increment, Use_leap_seconds enum('Y','N') DEFAULT 'N' NOT NULL,PRIMARY KEY TzId (Time_zone_id)) DEFAULT CHARACTER SET latin1 comment='Time zones';
CREATE TABLE time_zone_transition (Time_zone_id int unsigned NOT NULL,Transition_time bigint signed NOT NULL,Transition_type_id int unsigned NOT NULL,PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time)) DEFAULT CHARACTER SET latin1 comment='Time zone transitions';

CREATE TABLE time_zone_transition_type (Time_zone_id int unsigned NOT NULL,Transition_type_id int unsigned NOT NULL,Offset int signed DEFAULT 0 NOT NULL,Is_DST tinyint unsigned DEFAULT 0 NOT NULL,Abbreviation char(8) DEFAULT '' NOT NULL,PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id)) DEFAULT CHARACTER SET latin1 comment='Time zone transition types';
CREATE TABLE time_zone_leap_second (Transition_time bigint signed NOT NULL,Correction int signed NOT NULL,PRIMARY KEY TranTime (Transition_time)) DEFAULT CHARACTER SET latin1 comment='Leap seconds information for time zones';
