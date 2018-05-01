--echo ########################################################################
--echo # Verify the existence of the expected DD tables, neither less nor more.
--echo # Fill two help tables with the DD and DDSE table names.
--echo ########################################################################

--source include/have_debug.inc
SET debug = '+d,skip_dd_table_access_check';

--echo ########################################################################
--echo # The number of hidden DD/DDSE tables must be as expected.
--echo ########################################################################
let $number_of_hidden_dd_tables = 31;
let $assert_cond = "[SELECT COUNT(*) from mysql.tables
                       WHERE schema_id = 1 AND hidden = \'System\']"
                    = $number_of_hidden_dd_tables;
--let $assert_text = There are 31 hidden DD/DDSE tables.
--source include/assert.inc

# Fill two help tables with the names of the DDSE and DD tables.
# Unless the table names are set by the caller, create two tables
# temporarily and delete them at the end of this file.

--disable_query_log
--disable_result_log
if (!$ddse_table_names)
{
  let $ddse_table_names = test.ddse_table_names;
  eval CREATE TABLE $ddse_table_names (name VARCHAR(64) PRIMARY KEY);
  let $drop_ddse_table_names = $ddse_table_names;
}

if (!$dd_table_names)
{
  let $dd_table_names = test.dd_table_names;
  eval CREATE TABLE $dd_table_names (name VARCHAR(64) PRIMARY KEY);
  let $drop_dd_table_names = $dd_table_names;
}

eval INSERT INTO $ddse_table_names (name) VALUES
               ('innodb_ddl_log'),
               ('innodb_dynamic_metadata'),
               ('innodb_index_stats'),
               ('innodb_table_stats');

eval INSERT INTO $dd_table_names (name) VALUES
               ('catalogs'),
               ('character_sets'),
               ('collations'),
               ('column_statistics'),
               ('column_type_elements'),
               ('columns'),
               ('dd_properties'),
               ('events'),
               ('foreign_key_column_usage'),
               ('foreign_keys'),
               ('index_column_usage'),
               ('index_partitions'),
               ('index_stats'),
               ('indexes'),
               ('parameter_type_elements'),
               ('parameters'),
               ('resource_groups'),
               ('routines'),
               ('schemata'),
               ('st_spatial_reference_systems'),
               ('table_partition_values'),
               ('table_partitions'),
               ('table_stats'),
               ('tables'),
               ('tablespace_files'),
               ('tablespaces'),
               ('triggers'),
               ('view_routine_usage'),
               ('view_table_usage');
--enable_result_log
--enable_query_log

let $assert_cond = "[SELECT (SELECT COUNT(*) FROM $dd_table_names) +
                            (SELECT COUNT(*) FROM $ddse_table_names)]"
                    = $number_of_hidden_dd_tables + 2;
--let $assert_text = There are 33 DD/DDSE tables in total.
--source include/assert.inc

--echo ########################################################################
--echo # No unexpected DD tables must be present.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) FROM mysql.tables
                       WHERE schema_id = 1
                       AND hidden = \'System\'
                       AND name NOT IN (SELECT name FROM $dd_table_names)
                       AND name NOT IN (SELECT name FROM $ddse_table_names)]"
                    = 0;
--let $assert_text = No unexpected DD tables are present.
--source include/assert.inc

--echo ########################################################################
--echo # All expected DD tables must be present.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) FROM $dd_table_names
                       WHERE name NOT IN
                         (SELECT name FROM mysql.tables WHERE schema_id = 1)]"
                    = 0;
--let $assert_text = All expected DD tables are present.
--source include/assert.inc

--echo ########################################################################
--echo # All expected DDSE tables must be present.
--echo ########################################################################
let $assert_cond = "[SELECT COUNT(*) FROM $ddse_table_names
                       WHERE name NOT IN
                         (SELECT name FROM mysql.tables WHERE schema_id = 1)]"
                    = 0;
--let $assert_text = All expected DDSE are be present.
--source include/assert.inc

if ($drop_ddse_table_names)
{
  eval DROP TABLE $drop_ddse_table_names;
  let $ddse_table_names =;
  let $drop_ddse_table_names =;
}

if ($drop_dd_table_names)
{
  eval DROP TABLE $drop_dd_table_names;
  let $dd_table_names =;
  let $drop_dd_table_names =;
}

SET debug = '-d,skip_dd_table_access_check';
