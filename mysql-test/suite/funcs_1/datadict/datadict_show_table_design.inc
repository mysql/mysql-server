#### suite/funcs_1/datadict/datadict_show_table_design.test
#
# - shows design of *one* table from INFORMATION_SCHEMA
# - used to have identical 'view' on all tested tables
#
# Usage:
#
# let $is_table= <name of one of the tables>;
# --source suite/funcs_1/datadict/datadict_show_table_design.test

USE information_schema;

--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval DESC $is_table;

--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SHOW CREATE TABLE $is_table;

eval SELECT COUNT(*) FROM information_schema.columns
WHERE table_schema = 'information_schema'
  AND table_name   = '$is_table'
ORDER BY ordinal_position;

--source suite/funcs_1/datadict/datadict_bug_12777.inc
eval SELECT * FROM information_schema.columns
WHERE table_schema = 'information_schema'
  AND table_name   = '$is_table'
ORDER BY ordinal_position;
