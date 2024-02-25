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
--replace_column 10 X
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
--replace_column 10 X 11 X
  }
}
--disable_query_log
--disable_warnings
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
--enable_warnings
--enable_query_log

if ($json) {
if ($innodb) {
  if ($no_rows) {
--replace_regex /"rows_examined_per_scan": [0-9]+/"rows_examined_per_scan": "X"/ /_per_join": [0-9]+/_per_join": "X"/ /_per_join": "[0-9]+K"/_per_join": "X"/  /"filtered": "[0-9.]+"/"filtered": "X"/ /_cost": "[0-9.]+"/_cost": "X"/ /"data_read_per_join": "[0-9.]+"/"data_read_per_join": "X"/
}
}
--eval EXPLAIN FORMAT=JSON $query;
--disable_result_log
--exec $MYSQL -S $MASTER_MYSOCK -u root -r test -e "EXPLAIN FORMAT=JSON $query;" > $MYSQLTEST_VARDIR/tmp/explain.json
--exec perl $MYSQL_TEST_DIR/suite/opt_trace/validate_json.pl $MYSQLTEST_VARDIR/tmp/explain.json
--remove_file '$MYSQLTEST_VARDIR/tmp/explain.json'
--enable_result_log
}

if ($select) {
FLUSH STATUS;
FLUSH TABLES;
if ($innodb) {
  if ($no_rows) {
--replace_column 10 X 11 X
  }
}
--disable_query_log
--disable_warnings
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
--enable_warnings
--enable_query_log
if ($json) {
if ($innodb) {
  if ($no_rows) {
--replace_regex /"rows_examined_per_scan": [0-9]+/"rows_examined_per_scan": "X"/ /_per_join": [0-9]+/_per_join": "X"/ /_per_join": "[0-9]+K"/_per_join": "X"/  /"filtered": "[0-9.]+"/"filtered": "X"/ /_cost": "[0-9.]+"/_cost": "X"/ /"data_read_per_join": "[0-9.]+"/"data_read_per_join": "X"/
  }
}
--eval EXPLAIN FORMAT=JSON $select;
--disable_result_log
--exec $MYSQL -S $MASTER_MYSOCK -u root -r test -e "EXPLAIN FORMAT=JSON $select;" > $MYSQLTEST_VARDIR/tmp/explain.json
--exec perl $MYSQL_TEST_DIR/suite/opt_trace/validate_json.pl $MYSQLTEST_VARDIR/tmp/explain.json
--remove_file '$MYSQLTEST_VARDIR/tmp/explain.json'
--enable_result_log
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
--echo # This line exists only to help --skip_if_hypergraph diffing.
--echo # Status of "equivalent" SELECT query execution:
--disable_warnings
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;
--enable_warnings
}

FLUSH STATUS;
FLUSH TABLES;
--eval $query
--echo # This line exists only to help --skip_if_hypergraph diffing.
--echo # Status of testing query execution:
--disable_warnings
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS WHERE (Variable_name LIKE 'Sort%' OR
                   Variable_name LIKE 'Handler_read_%' OR
                   Variable_name = 'Handler_write' OR
                   Variable_name = 'Handler_update' OR
                   Variable_name = 'Handler_delete') AND Value <> 0;

--enable_warnings
--let $query=
--let $select=
--let $no_rows=

--enable_query_log

--echo
