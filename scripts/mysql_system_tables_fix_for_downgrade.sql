-- Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.
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

# This script will be used during downgrade from MySQL version 5.7 to 5.6. 
# This script will convert the system tables of 5.7 server to be compatible
# with 5.6 server.

SET session sql_log_bin=0;
SET NAMES 'utf8';
START TRANSACTION;

USE `mysql`;

--
-- Convert InnoDB tables to MyISAM
--

ALTER TABLE `engine_cost` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `gtid_executed` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `help_category` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `help_keyword` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `help_relation` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `help_topic` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `innodb_index_stats` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `innodb_table_stats` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `plugin` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `server_cost` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `servers` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `slave_master_info` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `slave_relay_log_info` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `slave_worker_info` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `time_zone` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `time_zone_leap_second` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `time_zone_name` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `time_zone_transition` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;
ALTER TABLE `time_zone_transition_type` ENGINE=MyISAM STATS_PERSISTENT=DEFAULT;

--
-- Check if password field is already there, if not add it and fix the values
--


SET @have_password= (SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'mysql'
                    AND TABLE_NAME='user'
                    AND column_name='password');
SET @str=IF(@have_password = 0, "ALTER TABLE user ADD Password char(41) character set latin1 collate latin1_bin NOT NULL default '' AFTER User", "SET @dummy = 0");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;
SET @str=IF(@have_password = 0, "UPDATE user SET password = authentication_string where LENGTH(authentication_string) = 41 and plugin = 'mysql_native_password'", "SET @dummy = 0");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;
SET @str=IF(@have_password = 0, "UPDATE user SET authentication_string = '' where LENGTH(authentication_string) = 41 and plugin = 'mysql_native_password'", "SET @dummy = 0");
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- Convert username length from 32 to 16 characters
--

-- WARNING! Below commands will fail if there is any user name with length
-- greater than 16 characters. Please make sure this is not the case.

SET session sql_mode='STRICT_ALL_TABLES';

ALTER TABLE tables_priv
  MODIFY User char(16) NOT NULL default '';
ALTER TABLE columns_priv
  MODIFY User char(16) NOT NULL default '';
ALTER TABLE user
  MODIFY User char(16) NOT NULL default '';
ALTER TABLE db
  MODIFY User char(16) NOT NULL default '';
ALTER TABLE procs_priv
  MODIFY User char(16) binary DEFAULT '' NOT NULL;

--
-- Downgrade performance_schema tables
--

set @have_pfs= (select count(engine) from information_schema.engines where engine='PERFORMANCE_SCHEMA' and support != 'NO');

--
-- TABLE USERS
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.users;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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

SET @cmd="DROP TABLE IF EXISTS performance_schema.accounts;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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

--
-- TABLE SETUP_ACTORS
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.setup_actors;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="CREATE TABLE performance_schema.setup_actors("
  "HOST CHAR(60) collate utf8_bin default '%' not null,"
  "USER CHAR(16) collate utf8_bin default '%' not null,"
  "ROLE CHAR(16) collate utf8_bin default '%' not null,"
  "ENABLED ENUM ('YES', 'NO') not null default 'YES',"
  "HISTORY ENUM ('YES', 'NO') not null default 'YES'"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_stages_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE EVENTS_WAITS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_waits_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_waits_summary_by_account_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_statements_summary_by_account_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE EVENTS_STATEMENTS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_statements_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE EVENTS_STAGES_SUMMARY_BY_ACCOUNT_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_stages_summary_by_account_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE EVENTS_STAGES_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_stages_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

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
-- TABLE THREADS
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.threads;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="CREATE TABLE performance_schema.threads("
  "THREAD_ID BIGINT unsigned not null,"
  "NAME VARCHAR(128) not null,"
  "TYPE VARCHAR(10) not null,"
  "PROCESSLIST_ID BIGINT unsigned,"
  "PROCESSLIST_USER VARCHAR(16),"
  "PROCESSLIST_HOST VARCHAR(60),"
  "PROCESSLIST_DB VARCHAR(64),"
  "PROCESSLIST_COMMAND VARCHAR(16),"
  "PROCESSLIST_TIME BIGINT,"
  "PROCESSLIST_STATE VARCHAR(64),"
  "PROCESSLIST_INFO LONGTEXT,"
  "PARENT_THREAD_ID BIGINT unsigned,"
  "ROLE VARCHAR(64),"
  "INSTRUMENTED ENUM ('YES', 'NO') not null,"
  "HISTORY ENUM ('YES', 'NO') not null"
  ")ENGINE=PERFORMANCE_SCHEMA;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

--
-- TABLE MEMORY_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.memory_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="CREATE TABLE performance_schema.memory_summary_by_user_by_event_name("
  "USER CHAR(16) collate utf8_bin default null,"
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

SET @cmd="DROP TABLE IF EXISTS performance_schema.memory_summary_by_account_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;


SET @cmd="CREATE TABLE performance_schema.memory_summary_by_account_by_event_name("
  "USER CHAR(16) collate utf8_bin default null,"
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
-- TABLE EVENTS_TRANSACTIONS_SUMMARY_BY_USER_BY_EVENT_NAME
--

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_transactions_summary_by_user_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_user_by_event_name("
  "USER CHAR(16) collate utf8_bin default null,"
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

SET @cmd="DROP TABLE IF EXISTS performance_schema.events_transactions_summary_by_account_by_event_name;";

SET @str = IF(@have_pfs = 1, @cmd, 'SET @dummy = 0');
PREPARE stmt FROM @str;
EXECUTE stmt;
DROP PREPARE stmt;

SET @cmd="CREATE TABLE performance_schema.events_transactions_summary_by_account_by_event_name("
  "USER CHAR(16) collate utf8_bin default null,"
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

COMMIT;
