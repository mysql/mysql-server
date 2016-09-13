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

# The DD database id
let $dd_schema_id= `SELECT id FROM schemata
  WHERE name= 'mysql'`;

# Expression representing the DD table ids
let $dd_table_ids=
  SELECT id FROM tables
  WHERE schema_id= $dd_schema_id
  AND name IN ('innodb_table_stats',
               'innodb_index_stats',
               'character_sets',
               'collations',
               'tablespaces',
               'tablespace_files',
               'catalogs',
               'schemata',
               'tables',
               'view_table_usage',
               'columns',
               'indexes',
               'index_column_usage',
               'column_type_elements',
               'foreign_keys',
               'foreign_key_column_usage',
               'table_partitions',
               'table_partition_values',
               'index_partitions',
               'version');

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
