-- Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
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

CREATE DEFINER=root@localhost PROCEDURE check_testcase_perfschema()
BEGIN
  IF ((SELECT count(*) from information_schema.engines
       where engine='PERFORMANCE_SCHEMA' and support='YES') = 1) THEN
  BEGIN

    BEGIN
      -- For tests tampering with performance_schema table structure
      DECLARE CONTINUE HANDLER for SQLEXCEPTION
      BEGIN
      END;

      -- Leave the instruments in the same state
      SELECT * from performance_schema.setup_instruments
        where enabled='NO' order by NAME;
    END;

    -- Leave the consumers in the same state
    SELECT * from performance_schema.setup_consumers
      order by NAME;

    -- Leave the actors setup in the same state
    SELECT * from performance_schema.setup_actors
      order by USER, HOST;

    -- Leave the objects setup in the same state
    SELECT * from performance_schema.setup_objects
      order by OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;
  END;
  END IF;
END||

--
-- Procedure used to check if server has been properly
-- restored after testcase has been run
--
CREATE DEFINER=root@localhost PROCEDURE check_testcase()
BEGIN

  CALL check_testcase_perfschema();

  -- Dump all global variables except those that may change.
  -- timestamp changes if time passes. server_uuid changes if server restarts.
  SELECT * FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES
    WHERE variable_name NOT IN ('timestamp', 'server_uuid',
                                'innodb_file_format_max',
                                'gtid_executed', 'gtid_purged')
    ORDER BY VARIABLE_NAME;

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

END||

--
-- Procedure used by test case used to force all
-- servers to restart after testcase and thus skipping
-- check test case after test
--
CREATE DEFINER=root@localhost PROCEDURE force_restart()
BEGIN
  SELECT 1 INTO OUTFILE 'force_restart';
END||
