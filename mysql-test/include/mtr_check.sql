-- Copyright (c) 2008, 2024, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is designed to work with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have either included with
-- the program or referenced in the documentation.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

delimiter ;

use mtr;

DELIMITER $$

CREATE DEFINER=root@localhost PROCEDURE check_testcase_perfschema()
BEGIN
  IF ((SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ count(*) from information_schema.engines
       where engine='PERFORMANCE_SCHEMA' and support='YES') = 1) THEN
  BEGIN

    BEGIN
      -- For tests tampering with performance_schema table structure
      DECLARE CONTINUE HANDLER for SQLEXCEPTION
      BEGIN
      END;

      -- Leave the instruments in the same state
      SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.setup_instruments
        where enabled='NO' order by NAME;
    END;

    -- Leave the consumers in the same state
    SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.setup_consumers
      order by NAME;

    -- Leave the actors setup in the same state
    SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.setup_actors
      order by USER, HOST;

    -- Leave the objects setup in the same state
    SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.setup_objects
      order by OBJECT_TYPE, OBJECT_SCHEMA, OBJECT_NAME;

    -- Leave the prepared statement instances in the same state
    SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.prepared_statements_instances;

    -- Leave the user defined functions in the same state
    SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * from performance_schema.user_defined_functions
      ORDER BY UDF_NAME;
  END;
  END IF;
END$$

CREATE DEFINER=root@localhost PROCEDURE check_testcase_ndb()
BEGIN
  -- Check NDB only if the ndbcluster plugin is enabled
  IF ((SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ count(*) FROM information_schema.engines
       WHERE engine='ndbcluster' AND support IN ('YES', 'DEFAULT')) = 1) THEN

    -- Check only if connection information is available
    IF (SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ LENGTH(VARIABLE_VALUE) > 0
          FROM performance_schema.global_variables
             WHERE VARIABLE_NAME='ndb_connectstring') THEN

      -- Check ndbinfo engine is enabled, else ndbinfo schema will not exists
      IF ((SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ count(*) FROM information_schema.engines
           WHERE engine='ndbinfo' AND support IN ('YES', 'DEFAULT')) = 1) THEN

        -- List dict objects in NDB, skip objects created by ndbcluster plugin
        -- and HashMap's since those can't be dropped after having been
        -- created by test
        SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ id, indented_name FROM ndbinfo.dict_obj_tree
          WHERE root_type != 24 AND  -- HashMap
                root_name NOT IN ( 'mysql/def/ndb_schema',
                                   'mysql/def/ndb_schema_result',
                                   'mysql/def/ndb_index_stat_head',
                                   'mysql/def/ndb_index_stat_sample',
                                   'mysql/def/ndb_apply_status',
                                   'mysql/def/ndb_sql_metadata')
          ORDER BY path;

      END IF;
    END IF;
  END IF;
END$$

-- Procedure used to check if server has been properly
-- restored after testcase has been run

CREATE DEFINER=root@localhost PROCEDURE check_testcase()
BEGIN

  CALL check_testcase_perfschema();

  CALL check_testcase_ndb();

  -- Dump all global variables except those that may change.
  -- timestamp changes if time passes. server_uuid changes if server restarts.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM performance_schema.global_variables
    WHERE variable_name NOT IN ('timestamp', 'server_uuid',
                                'gtid_executed', 'gtid_purged',
                                'group_replication_group_name',
                                'innodb_thread_sleep_delay')
  ORDER BY VARIABLE_NAME;

  -- Dump all persisted variables, those that may change.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM performance_schema.persisted_variables
    ORDER BY VARIABLE_NAME;

  -- Dump all databases, there should be none
  -- except those that was created during bootstrap
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM INFORMATION_SCHEMA.SCHEMATA ORDER BY SCHEMA_NAME;

  -- Dump all tablespaces, there should be none
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ FILE_NAME, FILE_TYPE, TABLESPACE_NAME, ENGINE FROM INFORMATION_SCHEMA.FILES
    WHERE FILE_TYPE !='TEMPORARY' ORDER BY FILE_NAME;

  -- The test database should not contain any tables
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ table_name AS tables_in_test FROM INFORMATION_SCHEMA.TABLES
    WHERE table_schema='test'
      ORDER BY TABLE_NAME;

  -- Show "mysql" database, tables and columns
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ CONCAT(table_schema, '.', table_name) AS tables_in_mysql
    FROM INFORMATION_SCHEMA.TABLES
      WHERE table_schema='mysql' AND table_name != 'ndb_apply_status'
        ORDER BY tables_in_mysql;
  -- Always use the old optimizer until bug#36553075 is fixed.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)
            SET_VAR(optimizer_switch='hypergraph_optimizer=off')*/
         CONCAT(table_schema, '.', table_name) AS columns_in_mysql,
       column_name, ordinal_position, column_default, is_nullable,
       data_type, character_maximum_length, character_octet_length,
       numeric_precision, numeric_scale, character_set_name,
       collation_name, column_type, column_key, extra, column_comment
    FROM INFORMATION_SCHEMA.COLUMNS
      WHERE table_schema='mysql' AND table_name != 'ndb_apply_status'
        ORDER BY columns_in_mysql, column_name;

  -- Dump all events, there should be none
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM INFORMATION_SCHEMA.EVENTS;

  -- Dump all triggers except mtr internals, only those in the sys schema should exist
  -- do not select the CREATED column however, as tests like mysqldump.test / mysql_ugprade.test update this
  -- Always use the old optimizer until bug#36553075 is fixed.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)
            SET_VAR(optimizer_switch='hypergraph_optimizer=off')*/
         TRIGGER_CATALOG, TRIGGER_SCHEMA, TRIGGER_NAME, EVENT_MANIPULATION,
         EVENT_OBJECT_CATALOG, EVENT_OBJECT_SCHEMA, EVENT_OBJECT_TABLE, ACTION_ORDER, ACTION_CONDITION,
         ACTION_STATEMENT, ACTION_ORIENTATION, ACTION_TIMING ACTION_REFERENCE_OLD_TABLE, ACTION_REFERENCE_NEW_TABLE,
         ACTION_REFERENCE_OLD_ROW, ACTION_REFERENCE_NEW_ROW, SQL_MODE, DEFINER CHARACTER_SET_CLIENT,
         COLLATION_CONNECTION, DATABASE_COLLATION
    FROM INFORMATION_SCHEMA.TRIGGERS
      WHERE TRIGGER_NAME NOT IN ('gs_insert', 'ts_insert')
      ORDER BY TRIGGER_CATALOG, TRIGGER_SCHEMA, TRIGGER_NAME;

  -- Dump all created procedures, only those in the sys schema should exist
  -- do not select the CREATED or LAST_ALTERED columns however, as tests like mysqldump.test / mysql_ugprade.test update this
  -- Always use the old optimizer until bug#36553075 is fixed.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)
            SET_VAR(optimizer_switch='hypergraph_optimizer=off')*/
         SPECIFIC_NAME,ROUTINE_CATALOG,ROUTINE_SCHEMA,ROUTINE_NAME,ROUTINE_TYPE,DATA_TYPE,CHARACTER_MAXIMUM_LENGTH,
         CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE,DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,
         DTD_IDENTIFIER,ROUTINE_BODY,ROUTINE_DEFINITION,EXTERNAL_NAME,EXTERNAL_LANGUAGE,PARAMETER_STYLE,
         IS_DETERMINISTIC,SQL_DATA_ACCESS,SQL_PATH,SECURITY_TYPE,SQL_MODE,ROUTINE_COMMENT,DEFINER,
         CHARACTER_SET_CLIENT,COLLATION_CONNECTION,DATABASE_COLLATION
    FROM INFORMATION_SCHEMA.ROUTINES ORDER BY ROUTINE_SCHEMA, ROUTINE_NAME, ROUTINE_TYPE;

  -- Dump all views, only those in the sys schema should exist
  -- Always use the old optimizer until bug#36553075 is fixed.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)
            SET_VAR(optimizer_switch='hypergraph_optimizer=off')*/
         * FROM INFORMATION_SCHEMA.VIEWS
    ORDER BY TABLE_SCHEMA, TABLE_NAME;

  -- Dump all plugins, loaded with plugin-loading options or through
  -- INSTALL/UNINSTALL command
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM INFORMATION_SCHEMA.PLUGINS;

  -- Leave InnoDB metrics in the same state
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ name, status FROM INFORMATION_SCHEMA.INNODB_METRICS
    ORDER BY name;

  SHOW GLOBAL STATUS LIKE 'replica_open_temp_tables';

  -- Check for number of active connections before & after the test run.

  -- mysql.session is used internally by plugins to access the server. We may
  -- not find consistent result in information_schema.processlist, hence
  -- excluding it from check-testcase. Similar reasoning applies to the event
  -- scheduler.
  --
  -- For "unauthenticated user", see Bug#30035699 "UNAUTHENTICATED USER" SHOWS UP IN CHECK-TESTCASE
  --
  -- Also (in similar fashion as above) exclude all 'Daemon' threads, they will
  -- not give consistent result either.
  --
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ USER, HOST, DB, COMMAND, INFO FROM INFORMATION_SCHEMA.PROCESSLIST
    WHERE COMMAND NOT IN ('Sleep', 'Daemon', 'Killed')
      AND USER NOT IN ('unauthenticated user','mysql.session', 'event_scheduler')
        ORDER BY COMMAND;

  -- Checksum system tables to make sure they have been properly
  -- restored after test.
  -- skip mysql.proc however, as created timestamps may have been updated by
  -- mysqldump.test / mysql_ugprade.test the above SELECT on I_S.ROUTINES
  -- ensures consistency across runs instead
  -- We are skipping mysql.plugin from the checksum table list, as it does not
  -- register plugin-loading options like (--plugin-load,--plugin-load-add ..)
  -- instead we will use I_S.PLUGINS to ensure consistency across runs.
  checksum table
    mysql.columns_priv,
    mysql.component,
    mysql.default_roles,
    mysql.db,
    mysql.func,
    mysql.general_log,
    mysql.global_grants,
    mysql.help_category,
    mysql.help_keyword,
    mysql.help_relation,
    mysql.help_topic,
    mysql.procs_priv,
    mysql.proxies_priv,
    mysql.replication_asynchronous_connection_failover,
    mysql.replication_asynchronous_connection_failover_managed,
    mysql.replication_group_configuration_version,
    mysql.replication_group_member_actions,
    mysql.role_edges,
    mysql.slow_log,
    mysql.tables_priv,
    mysql.time_zone,
    mysql.time_zone_leap_second,
    mysql.time_zone_name,
    mysql.time_zone_transition,
    mysql.time_zone_transition_type,
    mysql.user;

  -- Check that Replica IO Monitor thread state is the same before
  -- and after the test run, which is not running.
  SELECT /*+SET_VAR(use_secondary_engine=OFF)*/ * FROM performance_schema.threads WHERE NAME="thread/sql/replica_monitor";

END$$

-- Procedure used by test case used to force all
-- servers to restart after testcase and thus skipping
-- check test case after test
CREATE DEFINER=root@localhost PROCEDURE force_restart()
BEGIN
  SELECT 1 INTO OUTFILE 'force_restart';
END$$

DELIMITER ;

