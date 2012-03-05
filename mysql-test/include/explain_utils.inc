# This file is a collection of utility tests
# for WL#4897: Add EXPLAIN INSERT/UPDATE/DELETE.
#
# Since MTR doesn't have functions, we use this file instead
# including it many times.
#
# Parameters:
#
#  $query:   INSERT/REPLACE/UPDATE/DELETE query to explain
#            NOTE: this file resets this variable
#
#  $select:  may be empty; the SELECT query similar to $query
#            We use it to compare:
#              1) table data before and after EXPLAIN $query evaluation;
#              2) EXPLAIN $query and EXPLAIN $select output and
#                 handler/filesort statistics
#            NOTE: this file resets this variable
#  $innodb:  take $no_rows parameter into account if not 0;
#  $no_rows: filter out "rows" and "filtered" columns of EXPLAIN if not 0;
#            it may be necessary for InnoDB tables since InnoDB's table row
#            counter can't return precise and repeatable values;
#	     NOTE: ANALYZE TABLE doesn't help
#            NOTE: this file resets this variable

--echo #
--echo # query:  $query
--echo # select: $select
--echo #

if ($select) {
--disable_query_log
--eval $select INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/before_explain.txt'
--enable_query_log
}

if ($innodb) {
  if ($no_rows) {
--replace_column 9 X
  }
}
--eval EXPLAIN $query
if (`SELECT ROW_COUNT() > 0`) {
--echo # Erroneous query: EXPLAIN $query
--die Unexpected ROW_COUNT() <> 0
}

FLUSH STATUS;
FLUSH TABLES;
if ($innodb) {
  if ($no_rows) {
--replace_column 9 X 10 X
  }
}
--eval EXPLAIN EXTENDED $query
if (`SELECT ROW_COUNT() > 0`) {
--echo # Erroneous query: EXPLAIN EXTENDED $query
--die Unexpected ROW_COUNT() <> 0
}
--echo # Status of EXPLAIN EXTENDED query
--disable_query_log
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
--enable_query_log

if ($json) {
if ($innodb) {
  if ($no_rows) {
--replace_regex /"rows": [0-9]+/"rows": "X"/ /"filtered": [0-9.]+/"filtered": "X"/
  }
}
--eval EXPLAIN FORMAT=JSON $query;
if ($validation) {
--disable_query_log
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQL -S $MASTER_MYSOCK -u root -r test -e "EXPLAIN FORMAT=JSON $query;" > $MYSQLTEST_VARDIR/tmp/explain.json
--replace_regex  /[-]*// /FILE.[\/\\:_\.0-9A-Za-z]*/Validation:/
--exec python $MYSQL_TEST_DIR/suite/opt_trace/validate_json.py $MYSQLTEST_VARDIR/tmp/explain.json 
--remove_file '$MYSQLTEST_VARDIR/tmp/explain.json'
--enable_query_log
}
}

if ($select) {
FLUSH STATUS;
FLUSH TABLES;
if ($innodb) {
  if ($no_rows) {
--replace_column 9 X 10 X
  }
}
--eval EXPLAIN EXTENDED $select
--echo # Status of EXPLAIN EXTENDED "equivalent" SELECT query execution
--disable_query_log
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
--enable_query_log
if ($json) {
if ($innodb) {
  if ($no_rows) {
--replace_regex /"rows": [0-9]+/"rows": "X"/ /"filtered": [0-9.]+/"filtered": "X"/
  }
}
--eval EXPLAIN FORMAT=JSON $select;
if ($validation) {
--disable_query_log
--replace_result $MASTER_MYSOCK MASTER_MYSOCK
--exec $MYSQL -S $MASTER_MYSOCK -u root -r test -e "EXPLAIN FORMAT=JSON $select;" > $MYSQLTEST_VARDIR/tmp/explain.json
--replace_regex  /[-]*// /FILE.[\/\\:_\.0-9A-Za-z]*/Validation:/
--exec python $MYSQL_TEST_DIR/suite/opt_trace/validate_json.py $MYSQLTEST_VARDIR/tmp/explain.json 
--remove_file '$MYSQLTEST_VARDIR/tmp/explain.json'
--enable_query_log
}
}
}

--disable_query_log

if ($select) {
--eval $select INTO OUTFILE '$MYSQLTEST_VARDIR/tmp/after_explain.txt'
--diff_files '$MYSQLTEST_VARDIR/tmp/before_explain.txt' '$MYSQLTEST_VARDIR/tmp/after_explain.txt'
--remove_file '$MYSQLTEST_VARDIR/tmp/before_explain.txt'
--remove_file '$MYSQLTEST_VARDIR/tmp/after_explain.txt'
}

FLUSH STATUS;
FLUSH TABLES;
if ($select) {
--disable_result_log
--eval $select
--enable_result_log
--echo # Status of "equivalent" SELECT query execution:
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
}

FLUSH STATUS;
FLUSH TABLES;
--eval $query
--echo # Status of testing query execution:
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;

--let $query=
--let $select=
--let $no_rows=

--enable_query_log

--echo
