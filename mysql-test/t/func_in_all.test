--source include/no_valgrind_without_big.inc

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--source include/func_in.inc

set optimizer_switch=default;

