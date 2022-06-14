--echo ########################################################################
--echo # Verify the existence of the expected I_S system views, neither
--echo # less nor more. Fill help table with the I_S system view names.
--echo ########################################################################

--source include/have_debug.inc

--echo ########################################################################
--echo # The number of I_S system views must be as expected.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) from mysql.tables
                       WHERE type = \'SYSTEM VIEW\' AND
                       CHAR_LENGTH(view_definition_utf8)>0]"
                    = $expected_system_view_count;
--let $assert_text = There are $expected_system_view_count system views.
--source include/assert.inc

--disable_query_log
--disable_result_log
if (!$I_S_view_names)
{
  let $I_S_view_names = test.I_S_view_names;
  eval CREATE TABLE $I_S_view_names (name VARCHAR(64) PRIMARY KEY);
  let $drop_I_S_view_names = $I_S_view_names;
}

# Fill help tables with the names of the I_S system views.

eval INSERT INTO $I_S_view_names (name) VALUES
  ('CHARACTER_SETS'),
  ('CHECK_CONSTRAINTS'),
  ('COLLATIONS'),
  ('COLLATION_CHARACTER_SET_APPLICABILITY'),
  ('COLUMNS'),
  ('COLUMNS_EXTENSIONS'),
  ('COLUMN_STATISTICS'),
  ('EVENTS'),
  ('FILES'),
  ('INNODB_DATAFILES'),
  ('INNODB_FOREIGN'),
  ('INNODB_FOREIGN_COLS'),
  ('INNODB_FIELDS'),
  ('INNODB_TABLESPACES_BRIEF'),
  ('KEY_COLUMN_USAGE'),
  ('KEYWORDS'),
  ('PARAMETERS'),
  ('PARTITIONS'),
  ('REFERENTIAL_CONSTRAINTS'),
  ('RESOURCE_GROUPS'),
  ('ROUTINES'),
  ('SCHEMATA'),
  ('SCHEMATA_EXTENSIONS'),
  ('SHOW_STATISTICS'),
  ('ST_SPATIAL_REFERENCE_SYSTEMS'),
  ('ST_UNITS_OF_MEASURE'),
  ('ST_GEOMETRY_COLUMNS'),
  ('STATISTICS'),
  ('TABLE_CONSTRAINTS'),
  ('TABLE_CONSTRAINTS_EXTENSIONS'),
  ('TABLES'),
  ('TABLES_EXTENSIONS'),
  ('TABLESPACES_EXTENSIONS'),
  ('TRIGGERS'),
  ('USER_ATTRIBUTES'),
  ('VIEW_ROUTINE_USAGE'),
  ('VIEW_TABLE_USAGE'),
  ('VIEWS'),
  ('APPLICABLE_ROLES'),
  ('ADMINISTRABLE_ROLE_AUTHORIZATIONS'),
  ('ENABLED_ROLES'),
  ('ROLE_TABLE_GRANTS'),
  ('ROLE_COLUMN_GRANTS'),
  ('ROLE_ROUTINE_GRANTS');
--enable_result_log
--enable_query_log

let $assert_cond = "[SELECT COUNT(*) FROM $I_S_view_names]"
                    = $expected_system_view_count;
--let $assert_text = There are $expected_system_view_count I_S system views in total.
--source include/assert.inc

--echo ########################################################################
--echo # No unexpected I_S tables must be present.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) from mysql.tables
                       WHERE type = \'SYSTEM VIEW\' AND
                       CHAR_LENGTH(view_definition_utf8)>0 AND
                       name NOT IN (SELECT name FROM $I_S_view_names)]"
                    = 0;
--let $assert_text = No unexpected I_S system view are present.
--source include/assert.inc

--echo ########################################################################
--echo # All expected I_S system views should be present.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) FROM $I_S_view_names
                       WHERE name NOT IN
                       (SELECT name from mysql.tables
                       WHERE type = \'SYSTEM VIEW\' AND
                       CHAR_LENGTH(view_definition_utf8)>0)]"
                    = 0;
--let $assert_text = All expected I_S system views are present.
--source include/assert.inc

if ($drop_I_S_view_names)
{
  eval DROP TABLE $drop_I_S_view_names;
  let $I_S_view_names =;
  let $drop_I_S_view_names =;
}
