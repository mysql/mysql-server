#
# Verify definitions of DD tables.
#
# Check that hard-coded definitions of DD tables don't
# change if CREATE TABLE semantics changes. If this happens,
# and the results of this test change, then the hard-coded
# definitions of the DD tables need to be updated to match
# the old test results."

--source include/have_debug.inc

use mysql;

SET GLOBAL debug= '+d,skip_dd_table_access_check';

--echo #
--echo # List the CREATE TABLE statements for the DD tables.
--echo # Hide the AUTO INCREMENT counter, which is not
--echo # deterministic when running mtr.
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE catalogs;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE character_sets;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE collations;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE column_statistics;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE column_type_elements;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE columns;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE dd_properties;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE events;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE foreign_key_column_usage;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE foreign_keys;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE index_column_usage;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE index_partitions;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE index_stats;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE indexes;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE innodb_ddl_log;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE innodb_dynamic_metadata;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE innodb_index_stats;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE innodb_table_stats;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE parameter_type_elements;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE parameters;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE resource_groups;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE routines;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE schemata;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE st_spatial_reference_systems;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE table_partition_values;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE table_partitions;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE table_stats;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE tables;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE tablespace_files;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE tablespaces;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE triggers;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE view_routine_usage;
--replace_regex / AUTO_INCREMENT=[0-9]+//
SHOW CREATE TABLE view_table_usage;

# The ids of the dd_properties table must be fixed.
SELECT id = 1 FROM tables WHERE name LIKE 'dd_properties';
SELECT se_private_id = 1 FROM tables WHERE name LIKE 'dd_properties';
SELECT id = 1 FROM indexes WHERE table_id = (SELECT id FROM tables WHERE name LIKE 'dd_properties');

# The ids of the innodb_dynamic_metadata table must be fixed.
SELECT id = 2 FROM tables WHERE name LIKE 'innodb_dynamic_metadata';
SELECT se_private_id = 2 FROM tables WHERE name LIKE 'innodb_dynamic_metadata';
SELECT id = 2 FROM indexes WHERE table_id = (SELECT id FROM tables WHERE name LIKE 'innodb_dynamic_metadata');

# The DD database id must be fixed.
SELECT id = 1 FROM schemata WHERE name LIKE 'mysql';

# The DD tablespace id must be fixed.
SELECT id = 1 FROM tablespaces WHERE name LIKE 'mysql';

# The DD tablespace se private id must be fixed.
SELECT SUBSTRING_INDEX(
         SUBSTRING(se_private_data,
                   LOCATE('id', se_private_data),
                   30),
         ';', 1) = 'id=4294967294'
  FROM tablespaces WHERE name LIKE 'mysql';

# The DD database id
let $dd_schema_id= `SELECT id FROM schemata
  WHERE name= 'mysql'`;

# Expression representing the DD table ids
let $dd_table_ids=
  SELECT id FROM tables
  WHERE schema_id= $dd_schema_id
  AND name IN ('catalogs',
               'character_sets',
               'collations',
               'column_statistics',
               'column_type_elements',
               'columns',
               'dd_properties',
               'events',
               'foreign_key_column_usage',
               'foreign_keys',
               'index_column_usage',
               'index_partitions',
               'index_stats',
               'indexes',
               'innodb_ddl_log',
               'innodb_dynamic_metadata',
               'innodb_index_stats',
               'innodb_table_stats',
               'parameter_type_elements',
               'parameters',
               'resource_groups',
               'routines',
               'schemata',
               'st_spatial_reference_systems',
               'table_partition_values',
               'table_partitions',
               'table_stats',
               'tables',
               'tablespace_files',
               'tablespaces',
               'triggers',
               'view_routine_usage',
               'view_table_usage');

# Expression representing the DD index ids
let $dd_index_ids= SELECT id FROM indexes
  WHERE table_id IN ($dd_table_ids);

--echo #
--echo # Subset of definitions from tables, not including
--echo # timestamps, partitioning, view definitions and
--echo # default values
eval SELECT
    id, name, type, engine, collation_id, comment, hidden,
    options, se_private_data, se_private_id, tablespace_id
  FROM tables
  WHERE id IN ($dd_table_ids)
  ORDER BY id;

--echo #
--echo # Subset of definitions from columns, not including
--echo # default_values.
eval SELECT id, table_id, name, ordinal_position, type, is_nullable,
    is_zerofill, is_unsigned, char_length, numeric_precision,
    numeric_scale, datetime_precision, collation_id, default_option,
    update_option, is_auto_increment, comment, hidden, options,
   se_private_data FROM columns
  WHERE table_id IN ($dd_table_ids)
  ORDER BY id;

--echo #
--echo # Definitions from indexes
eval SELECT * FROM indexes
  WHERE table_id IN ($dd_table_ids)
  ORDER BY id;

--echo #
--echo # Definitions from index_column_usage
eval SELECT * FROM index_column_usage
  WHERE index_id IN ($dd_index_ids)
  ORDER BY index_id, column_id;

SET GLOBAL debug= '-d,skip_dd_table_access_check';
