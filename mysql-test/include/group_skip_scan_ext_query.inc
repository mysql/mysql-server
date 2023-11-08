# This query uses group skip scan.
let $hint = ;
let $check_query = $query $hint $cond;
--replace_regex $elide_costs
eval EXPLAIN FORMAT=TREE $check_query;
FLUSH STATUS;
--sorted_result
eval $check_query;
--echo 
--skip_if_hypergraph  # Depends on the query plan.
SHOW STATUS LIKE 'handler_read%';
--disable_query_log
eval create table group_query $check_query;
--enable_query_log

# This query doesn't use loose index scan.
let $hint= IGNORE INDEX(k1) ;
let $check_query = $query $hint $cond;
--disable_query_log
eval create table no_group_query $check_query;
--enable_query_log

# Check result diffence between tables.
--let $diff_tables= test.group_query, test.no_group_query
--source include/diff_tables.inc
drop tables test.group_query, test.no_group_query;
