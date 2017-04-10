# include/expect_qep.inc
#
# SUMMARY
#
#    Designed to be used together with include/check_qep.inc
#
#    $query should be assigned a select statement using 
#    straight_join to force the tables to be joined in most 
#    optimal order.
#
#    expect_qep.inc will then store the estimated 'Last_query_cost'
#    and total # 'Handler_read%' for this straight_joined query.
#
#    We should then assign a non-straight_join'ed version of
#    the same query to $query and execute it using 
#    'include/check_qep.inc'. Its estimated cost and
#    #handler_reads will then be verified against the
#    previous straight_joined query.
#
# USAGE
#
#    let $query= <select straight_join optimal statement>;
#    --source include/expect_qep.inc
#    let $query= <select statement>;
#    --source include/check_qep.inc
#
# EXAMPLE
#    t/greedy_optimizer.test
#

flush status;
eval EXPLAIN $query;
eval $query;

let $best_cost=
  query_get_value(SHOW STATUS LIKE 'Last_query_cost', Value, 1);

--disable_warnings
let $best_reads=
`select sum(variable_value)
   from performance_schema.session_status
   where VARIABLE_NAME like 'Handler_read%'`;
--enable_warnings

#echo Expect, cost: $best_cost, Handler_reads: $best_reads;

