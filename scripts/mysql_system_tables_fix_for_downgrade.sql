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

COMMIT;
