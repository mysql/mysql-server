#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
# 
# Run select.inc with all of the so-called 6.0 features.
#

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--source include/select.inc

set optimizer_switch=default;
