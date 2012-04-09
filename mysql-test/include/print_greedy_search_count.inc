# include/print_greedy_search_count.inc
#
# SUMMARY
#
#    $query should be assigned a select statement. 
#    The query will be EXPLAINed and the number of 
#    partial plans greedy search went through to 
#    create the QEP is printed.
#
# USAGE
#
#    let $query= <query>;
#    --source include/print_greedy_search_count.inc
#
# EXAMPLE
#    t/greedy_search.test
#

eval EXPLAIN $query;

let $greedy_search_partial_plans=
  query_get_value(SHOW STATUS LIKE 'Last_query_partial_plans', Value, 1);

--echo ### Partial_plans: $greedy_search_partial_plans
FLUSH STATUS;
