# 
# Run subquery_mat.inc with all of the so-called 6.0 features.
#

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on';

--source include/subquery_mat.inc

set optimizer_switch=default;
