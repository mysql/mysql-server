# Subroutine script used by query_expression_big.test . It's raison d'être
# is to allow repeating the tests below for both INTERSECT and EXCEPT
# without redundancy.
#
# Input:  $query - a variable containing the SQL command to be analyzed
# Output: stdout only
#
--source include/elide_costs_and_rows.inc

#
# Communicating with Perl subroutine in query_expression_record_elapsed.inc
# Input: SQL statements to run with EXPLAIN ANALYZE are communicated via a file:
#
--let COMMANDS = $MYSQLTEST_VARDIR/tmp/commands.sql
#
# Timings retrieved from the execution are to be found in this
# SQL table, put there by the Perl subroutine by parsing the result
# of the command, expected to be a EXPLAIN ANALYZE, possibly prepended with
# some SET variable commands.
CREATE TABLE timings(id INT AUTO_INCREMENT PRIMARY KEY, t FLOAT);

--echo # This will spill to disk with default TempTable size while
--echo # reading right table. 3rd fastest.
eval $query;
--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=tree $query;
exec echo "EXPLAIN ANALYZE $query" > $COMMANDS;
--source include/query_expression_record_elapsed.inc

--echo # Should not spill to disk. Fastest or 2nd fastest.
set internal_tmp_mem_storage_engine = MEMORY;
eval $query;
--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=tree $query;
exec echo "SET internal_tmp_mem_storage_engine = MEMORY;
           EXPLAIN ANALYZE $query" > $COMMANDS;
--source include/query_expression_record_elapsed.inc

#
--echo # This will spill to disk while reading left table. Slowest.
set internal_tmp_mem_storage_engine = default;
set max_heap_table_size=1024*2014;
set tmp_table_size = @@max_heap_table_size;

eval $query;
--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=tree $query;
exec echo "set max_heap_table_size=1024*2014;
           set tmp_table_size = @@max_heap_table_size;
           EXPLAIN ANALYZE $query" > $COMMANDS;
--source include/query_expression_record_elapsed.inc

--echo # Should not spill to disk. 2nd fastest or fastest.
set max_heap_table_size=1024*2014*1024;
set tmp_table_size = @@max_heap_table_size;

eval $query;
--replace_regex $elide_costs_and_rows
eval EXPLAIN FORMAT=tree $query;
exec echo "set max_heap_table_size=1024*2014*1024;
           set tmp_table_size = @@max_heap_table_size;
           EXPLAIN ANALYZE $query" > $COMMANDS;
--source include/query_expression_record_elapsed.inc

# Assert the expected ordering of timings
#
# Sometimes we see MEMORY (#2) winning, other times we see TempTable
# (#4) winning, so, merge those states by omitting them. We should
# see only the two slow scenarios: 1 and 3, in that order.
SELECT id FROM timings ORDER BY t LIMIT 2 OFFSET 2;

# clean up
set tmp_table_size = default;
set internal_tmp_mem_storage_engine = default;
DROP TABLE timings;
--remove_file $COMMANDS
