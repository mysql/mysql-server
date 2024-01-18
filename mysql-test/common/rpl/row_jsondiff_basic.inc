# ==== Requirements ====
#
# See common/rpl/row_jsondiff_init.inc for an overview.
#
# This script sets up the following scenarios:
# - updates using all supported JSON functions: JSON_SET, JSON_INSERT,
#   JSON_REMOVE
# - updates that generate different diff 'operations' (see
#   enum_json_diff_operation in json_diff.h)
# - updates that are logged in full format
# - updates that generate multiple diffs
# - updates componsed of several function calls
# - updates from non-NULL to NULL or NULL to non-NULL
# - updates that result in a no-op
# - updates that result in auto-wrapping (see the manual for JSON_SET)
# - updates where multiple rows are updated and:
#   - all are partial
#   - full appears before partial
#   - partial appears before full
#   - all are full
#
# ==== Usage ====
#
# Before using this script, setup the servers using like in
# rpl_row_jsondiff_basic.test:
# - setup replication
# - setup connections
# - set BINLOG_ROW_IMAGE and BINLOG_ROW_VALUE_OPTIONS as in rpl_row_jsondiff
# Then use this script like:
#
# [--let $key= KEY DEFINITION]
# --source include/rpl/row_jsondiff_basic.inc
#
# Parameters:
#
#   $key
#     This can be set to a key definition. It will be inserted verbatim
#     after the first INT column in the table definition.
#
# ==== See also ====
#
# rpl_row_jsondiff.test


--echo ******** Single JSON column ********

--echo ==== Full format ====

--let $stmt_pre= UPDATE t SET j =
--let $stmt_post= WHERE i = 1

--let $column_def= i INT $key, j JSON, k JSON
--let $rows= (1, '[1, 2]', '[3, 4]')
--let $echo_select= i, j, k
--let $compare_size_columns= j, k

--let $desc= Update one row using full format (wrong column used)
--let $stmt= JSON_SET(k, '$dollar[0]', 'a')
--source $scenario_inc

--let $column_def= i INT $key, j JSON
--let $rows= (1, '[1, 2]')
--let $echo_select= i, j
--let $compare_size_columns= j

--let $desc= Update one row using full format (no JSON function used)
--let $stmt= CAST(7 AS JSON)
--source $scenario_inc

--let $desc= Update one row using full format (wrong JSON function used (1))
--let $stmt= JSON_MERGE(j, '[8]')
--source $scenario_inc

--let $desc= Update one row using full format (wrong JSON function used (2))
--let $stmt= JSON_MERGE(JSON_REPLACE(j, '$dollar[0]', 3), '[8]')
--source $scenario_inc

--let $desc= Update one row using full format (wrong JSON function used (3))
--let $stmt= JSON_REPLACE(JSON_MERGE(j, '[8]'), '$dollar[0]', 3)
--source $scenario_inc

--let $desc= Update one row using full format (full smaller than partial)
--let $stmt= JSON_SET(j, '$dollar[0]', 'abcdefghijklmnopqrstuvwxyz', '$dollar[0]', 3)
--source $scenario_inc

--echo ==== Single diffs ====

--let $rows= (1, '[1, {"a": 2}]')

--let $desc= Update one row (REPLACE using JSON_REPLACE in array)
--let $stmt= JSON_REPLACE(j, '$dollar[0]', 7)
--source $scenario_inc

--let $desc= Update one row (REPLACE using JSON_SET in array)
--let $stmt= JSON_SET(j, '$dollar[0]', "7")
--source $scenario_inc

--let $desc= Update one row (REPLACE using JSON_REPLACE in object)
--let $stmt= JSON_REPLACE(j, '$dollar[1].a', CAST('[7]' AS JSON))
--source $scenario_inc

--let $desc= Update one row (REPLACE using JSON_SET in object)
--let $stmt= JSON_SET(j, '$dollar[1].a', CAST('{"a":7}' AS JSON))
--source $scenario_inc

--let $desc= Update one row (INSERT using JSON_SET in array)
--let $stmt= JSON_SET(j, '$dollar[2]', 3)
--source $scenario_inc

--let $desc= Update one row (INSERT using JSON_SET in object)
# decoder will output the number as int even if it is decimal, and this
# results in different sizes. so we skip the size comparison
--let $compare_size_columns=
--let $stmt= JSON_SET(j, '$dollar[1].b', CAST(4 AS DECIMAL))
--source $scenario_inc
--let $compare_size_columns= j

--let $desc= Update one row (REMOVE using JSON_REMOVE in array)
--let $stmt= JSON_REMOVE(j, '$dollar[1]')
--source $scenario_inc

--let $desc= Update one row (REMOVE using JSON_REMOVE in object)
--let $stmt= JSON_REMOVE(j, '$dollar[1].a')
--source $scenario_inc

--echo ==== Multiple diffs from one function call ====

--let $rows= (1, '[1, 2, {"a long string that ensures that the diff format is smaller than the full format": "", "a": 3, "b": 4}]')

--let $desc= Update one row (many different update types, using JSON_SET)
--let $stmt= JSON_SET(j, '$dollar[0]', 'one', '$dollar[1]', 'two', '$dollar[2].a', 'three', '$dollar[2].b', 'four', '$dollar[2].c', 'five', '$dollar[2].d', 'six', '$dollar[3]', 'seven', '$dollar[4]', 'eight')
--source $scenario_inc

--let $desc= Update one row (many different update types, using JSON_REPLACE)
--let $stmt= JSON_REPLACE(j, '$dollar[0]', 'one', '$dollar[1]', 'two', '$dollar[3].a', 'three', '$dollar[3].b', 'four', '$dollar[0]', 'ONE', '$dollar[1]', 'TWO', '$dollar[3].a', 'THREE', '$dollar[3].b', 'FOUR')
--source $scenario_inc

--let $desc= Update one row (using JSON_REMOVE)
--let $stmt= JSON_REMOVE(j, '$dollar[1]', '$dollar[2].b', '$dollar[2].a', '$dollar[0]')
--source $scenario_inc

--echo ==== Multiple diffs from multiple function calls ====

--let $rows= (1, '[1, 2, 3, "a long string that ensures that the diff format is smaller than the full format"]')

--let $desc= Update one row (using JSON_SET, JSON_REPLACE, and JSON_REMOVE)
--let $stmt= JSON_REMOVE(JSON_SET(JSON_REPLACE(JSON_SET(j, '$dollar[0]', 'one'), '$dollar[1]', 'two', '$dollar[2]', CAST('{"a": 1}' AS JSON)), '$dollar[2].b', 2, '$dollar[2].c', 3), '$dollar[0]', '$dollar[0]')
--source $scenario_inc

--echo ==== NULL values ====

--let $rows= (1, '[1, 2, 3, 4]')

--let $desc= Updating from non-NULL to NULL using JSON_SET
--let $stmt= JSON_SET(j, NULL, 'a')
--source $scenario_inc

--let $desc= Updating from non-NULL to NULL using JSON_REPLACE
--let $stmt= JSON_REPLACE(j, NULL, 'a')
--source $scenario_inc

--let $desc= Updating from non-NULL to NULL using JSON_REMOVE
--let $stmt= JSON_REMOVE(j, NULL)
--source $scenario_inc

--let $rows= (1, NULL)

--let $desc= Updating from NULL to non-NULL using JSON_SET
--let $stmt= JSON_SET('{"a": "a"}', '$dollar.b', j)
--source $scenario_inc

--let $desc= Updating from NULL to non-NULL using JSON_REPLACE
--let $stmt= JSON_REPLACE('{"a": "a"}', '$dollar.a', j)
--source $scenario_inc

--let $desc= Updating from NULL to non-NULL using JSON_REMOVE
--let $stmt= JSON_REPLACE('{"a": "a", "b": "b"}', '$dollar.b', 'BB')
--source $scenario_inc

--echo ==== No-op updates ====

--let $column_def= i INT $key, j JSON, x INT
--let $echo_select= i, j, x
--let $compare_size_columns= j
--let $rows= (1, '["a", {"b": "c"}]', 1)

# Change value of 'x' so that the row is not omitted
--let $stmt_pre= UPDATE t SET x = 2

--let $desc= Making a no-op not mentioning the column
--let $stmt=
--source $scenario_inc

# In the following tests set j to itself (expressed in different ways)
--let $stmt_pre= UPDATE t SET x = 2, j =

--let $desc= Making a no-op setting the column to itself
--let $stmt= j
--source $scenario_inc

--let $desc= Making a no-op setting the column to its own value (1)
--let $stmt= CONCAT(j, '')
--source $scenario_inc

--let $desc= Making a no-op setting the column to its own value (2)
--let $stmt= '["a", {"b": "c"}]'
--source $scenario_inc

--let $desc= Making a no-op using JSON_SET inserting object in non-object or array element in non-array
--let $stmt= JSON_SET(j, '$dollar.a', 0, '$dollar[1][2]', 1, '$dollar[1].b.x', 2)
--source $scenario_inc

--let $desc= Making a no-op using JSON_REPLACE with non-existing paths
--let $stmt= JSON_REPLACE(j, '$dollar[9]', 0, '$dollar.x', 1, '$dollar[1].x', 2)
--source $scenario_inc

--let $desc= Making a no-op removing a non-existing path, using JSON_REMOVE
--let $stmt= JSON_REMOVE(j, '$dollar[9]', '$dollar[1].x', '$dollar.foo', '$dollar[1][0]')
--source $scenario_inc

--let $desc= Making a no-op replacing a JSON value by itself, using JSON_SET
--let $stmt= JSON_SET(j, '$dollar[0]', 'a', '$dollar[1].b', 'c', '$dollar[0]', 'a')
--source $scenario_inc

--let $desc= Making a no-op replacing a JSON value by itself, using JSON_REPLACE
--let $stmt= JSON_REPLACE(j, '$dollar[0]', 'a', '$dollar[1].b', 'c', '$dollar[0]', 'a')
--source $scenario_inc

--let $desc= Making a no-op update using JSON_SET, JSON_REPLACE, and JSON_REMOVE
let $stmt=
  JSON_SET(JSON_REMOVE(JSON_REPLACE(j,
    '$dollar[0]', 'a', '$dollar[1].b', 'c', '$dollar[0]', 'a'),
    '$dollar[9]', '$dollar[1].x', '$dollar.foo'),
    '$dollar[0]', 'a', '$dollar[1].b', 'c', '$dollar[0]', 'a');
--source $scenario_inc

--echo ==== Long paths ====

--let $column_def= i INT $key, j JSON
--let $echo_select= i, j
--let $compare_size_columns= j
--let $stmt_pre= UPDATE t SET j =

--let $short_text= short text
--let $long_text= `SELECT REPEAT('a', 260)`
--let $very_long_text= `SELECT REPEAT('a', 66000)`

--let $desc= Update using long path
--let $rows= (1, '{"abc" : "def"}')
--let $stmt= JSON_SET(j, '$dollar.$long_text', 'ghi')
--source $scenario_inc

--let $desc= Update using long document
--let $rows= (1, '{"abc" : "def"}')
--let $stmt= JSON_SET(j, '$dollar.ghi', '$long_text')
--source $scenario_inc

--let $echo_stmt= 0
--let $echo_decoded_rows= 0
--let $echo_select=

--let $desc= Update using very long document
--let $rows= (1, '{"abc" : "def"}')
--let $stmt= JSON_SET(j, '$dollar.ghi', '$very_long_text')
--source $scenario_inc

--let $echo_stmt= 1
--let $echo_decoded_rows= 1

--echo ==== Auto-wrapping ====

--let $column_def= i INT $key, j JSON
--let $echo_select= i, j
--let $compare_size_columns= j
--let $rows= (1, '{"a" : 1}')
--let $stmt_pre= UPDATE t SET j =

--let $desc= Auto-wrap using JSON_SET
--let $stmt= JSON_SET(j, '$dollar[1]', 4711)
--source $scenario_inc

--echo ==== Multi-row updates ====

let $rows=
  (1, '[1, 2, 3]'),
  (2, '[4, 5, 6, "a long string that ensures that the diff format is smaller than the full format"]'),
  (3, '[7, 8, 9]'),
  (4, '[10, 11, 12, "a long string that ensures that the diff format is smaller than the full format"]');

--let $stmt_pre= UPDATE t SET j = JSON_SET(j, '$dollar[0]', 'abcdefghijklmnopqrstuvxyz', '$dollar[0]', 0) WHERE
--let $stmt_post=

--let $desc= Update two rows: both partial
--let $stmt= i = 2 OR i = 4
--source $scenario_inc

--let $desc= Update two rows: first partial, then full
--let $stmt= i = 2 OR i = 3
--source $scenario_inc

--let $desc= Update two rows: first full, then partial
--let $stmt= i = 1 OR i = 2
--source $scenario_inc

--let $desc= Update two rows: both full
--let $stmt= i = 1 OR i = 3
--source $scenario_inc

--let $desc= Update four rows: full, partial, full, partial
--let $stmt= i IN (1, 2, 3, 4)
--source $scenario_inc

--echo ==== Invalid paths ====

--echo TODO

--echo ******** Multiple JSON columns ********

--let $column_def= i INT $key, j JSON, k JSON, l JSON, m JSON
let $rows=
  (1,
   '[1, 2, 3, 4]',
   '[5, 6, 7, 8]',
   '{"a": 1, "b": 2, "c": 3, "d": 4}',
   '{"e": 5, "f": 6, "g": 7, "e": 8}');
--let $echo_select= i, j, k, l, m
--let $compare_size_columns= j, k, l, m
--let $stmt_pre= UPDATE t SET

--let $desc= Update after null and after non-modified column
--let $stmt= j = NULL, l = JSON_SET(l, '$dollar.d', "DDD")
--source $scenario_inc

--echo ******** Differences between master and slave ********

--echo TODO: applies ok, does not apply, and mix
