--source include/elide_costs.inc

set optimizer_switch = 'skip_scan=on';
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
FLUSH STATUS;
--disable_result_log
--eval $query
--enable_result_log
SHOW STATUS LIKE 'handler_read%';
--disable_query_log
--eval CREATE TABLE skip_scan $query
--enable_query_log

set optimizer_switch = 'skip_scan=off';
--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $query
FLUSH STATUS;
--disable_result_log
--eval $query
--enable_result_log
SHOW STATUS LIKE 'handler_read%';
--disable_query_log
--eval CREATE TABLE no_skip_scan $query
--enable_query_log

--let $diff_tables= test.skip_scan, test.no_skip_scan
--source include/diff_tables.inc

--replace_regex $elide_costs
--eval EXPLAIN FORMAT=TREE $hint_query
FLUSH STATUS;
--disable_result_log
--eval $hint_query
--enable_result_log
SHOW STATUS LIKE 'handler_read%';
--disable_query_log
--eval CREATE TABLE hint_skip_scan $hint_query
--enable_query_log

--let $diff_tables= test.hint_skip_scan, test.no_skip_scan
--source include/diff_tables.inc

--disable_query_log
DROP TABLE skip_scan, no_skip_scan, hint_skip_scan;
--enable_query_log
