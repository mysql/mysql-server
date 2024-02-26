# 
# Run subquery_sj_innodb.inc with all of the so-called 6.0 features.
#


set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--source include/subquery_sj_innodb.inc

set optimizer_switch=default;

