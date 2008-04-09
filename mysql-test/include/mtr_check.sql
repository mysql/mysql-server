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
    WHERE variable_name != 'timestamp';

  -- Dump all databases, there should be none
  -- except those that was created during bootstrap
  SELECT * FROM INFORMATION_SCHEMA.SCHEMATA;

  -- The test database should not contain any tables
  SELECT table_name AS tables_in_test FROM INFORMATION_SCHEMA.TABLES
    WHERE table_schema='test';

  -- Show "mysql" database, tables and columns
  SELECT table_name, table_type, engine FROM INFORMATION_SCHEMA.TABLES
    WHERE table_schema='mysql';
  SELECT * FROM INFORMATION_SCHEMA.COLUMNS
    WHERE table_schema='mysql';

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
