#### suite/funcs_1/datadict/basics_mixed3.inc
#
# Auxiliary routine to be sourced by suite/funcs_1/t/is_basics_mixed.test
#
# Check if INFORMATION_SCHEMA tables contain a schema_name like 'db_data%'.
#
# Author:
# 2008-01-23 mleich WL#4203 Reorganize and fix the data dictionary tests of
#                           testsuite funcs_1
#                   Create this script based on older scripts and new code.
#

# No column with the name of a database contained in:
#    character_sets collations collation_character_set_applicability
#    user_privileges
SELECT DISTINCT table_schema FROM information_schema.columns
WHERE table_schema LIKE 'db_data%';
SELECT DISTINCT table_schema FROM information_schema.column_privileges
WHERE table_schema LIKE 'db_data%';
SELECT DISTINCT constraint_schema,table_schema
FROM information_schema.key_column_usage
WHERE constraint_schema LIKE 'db_data%' OR table_schema LIKE 'db_data%';
SELECT DISTINCT routine_schema FROM information_schema.routines
WHERE routine_schema LIKE 'db_data%';
SELECT DISTINCT schema_name FROM information_schema.schemata
WHERE schema_name LIKE 'db_data%';
SELECT DISTINCT table_schema FROM information_schema.schema_privileges
WHERE table_schema LIKE 'db_data%';
SELECT DISTINCT table_schema,index_schema FROM information_schema.statistics
WHERE table_schema LIKE 'db_data%' OR index_schema LIKE 'db_data%';
SELECT DISTINCT table_schema FROM information_schema.tables
WHERE table_schema LIKE 'db_data%';
SELECT DISTINCT constraint_schema,table_schema
FROM information_schema.table_constraints
WHERE constraint_schema LIKE 'db_data%' OR table_schema LIKE 'db_data%';
SELECT DISTINCT table_schema FROM information_schema.table_privileges
WHERE table_schema LIKE 'db_data%';
SELECT DISTINCT trigger_schema,event_object_schema
FROM information_schema.triggers
WHERE trigger_schema LIKE 'db_data%' OR event_object_schema LIKE 'db_data%';
SELECT DISTINCT table_schema FROM information_schema.views
WHERE table_schema LIKE 'db_data%';
