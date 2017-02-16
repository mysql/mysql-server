-- Copyright (c) 2008, 2013, Oracle and/or its affiliates
-- Copyright (c) 2009, 2013, SkySQL Ab
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
-- along with this program; if not, write to the Free Software Foundation,
-- 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

delimiter ||;

use mtr||

--
-- Procedure used to check if server has been properly
-- restored after testcase has been run
--
CREATE DEFINER=root@localhost PROCEDURE check_testcase()
BEGIN

  -- Dump all global variables except those
  -- that are supposed to change
  SELECT * FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
    WHERE variable_name NOT IN ('timestamp', 'innodb_file_format_max')
     AND variable_name not like "Last_IO_Err*"
     AND variable_name != 'INNODB_IBUF_MAX_SIZE'
     AND variable_name != 'INNODB_USE_NATIVE_AIO'
   ORDER BY variable_name;

  -- Dump all databases, there should be none
  -- except those that was created during bootstrap
  SELECT * FROM INFORMATION_SCHEMA.SCHEMATA;

  -- The test database should not contain any tables
  SELECT table_name AS tables_in_test FROM INFORMATION_SCHEMA.TABLES
    WHERE table_schema='test';

  -- Show "mysql" database, tables and columns
  SELECT CONCAT(table_schema, '.', table_name) AS tables_in_mysql
    FROM INFORMATION_SCHEMA.TABLES
      WHERE table_schema='mysql' AND table_name != 'ndb_apply_status'
        ORDER BY tables_in_mysql;
  SELECT CONCAT(table_schema, '.', table_name) AS columns_in_mysql,
  	 column_name, ordinal_position, column_default, is_nullable,
         data_type, character_maximum_length, character_octet_length,
         numeric_precision, numeric_scale, character_set_name,
         collation_name, column_type, column_key, extra, column_comment
    FROM INFORMATION_SCHEMA.COLUMNS
      WHERE table_schema='mysql' AND table_name != 'ndb_apply_status'
        ORDER BY columns_in_mysql;

  -- Dump all events, there should be none
  SELECT * FROM INFORMATION_SCHEMA.EVENTS;
  -- Dump all triggers	 except mtr internals, there should be none
  SELECT * FROM INFORMATION_SCHEMA.TRIGGERS
         WHERE TRIGGER_NAME NOT IN ('gs_insert', 'ts_insert');
  -- Dump all created procedures, there should be none
  SELECT * FROM INFORMATION_SCHEMA.ROUTINES;

  SHOW STATUS LIKE 'slave_open_temp_tables';

  -- Checksum system tables to make sure they have been properly
  -- restored after test
  checksum table
    mysql.columns_priv,
    mysql.db,
    mysql.func,
    mysql.help_category,
    mysql.help_keyword,
    mysql.help_relation,
    mysql.host,
    mysql.proc,
    mysql.procs_priv,
    mysql.tables_priv,
    mysql.time_zone,
    mysql.time_zone_leap_second,
    mysql.time_zone_name,
    mysql.time_zone_transition,
    mysql.time_zone_transition_type,
    mysql.user;

  -- verify that no plugin changed its disabled/enabled state
  SELECT * FROM INFORMATION_SCHEMA.PLUGINS;

  select * from information_schema.session_variables
    where variable_name = 'debug_sync';

END||

