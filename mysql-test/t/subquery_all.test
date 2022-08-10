# ER_STACK_OVERRUN_NEED_MORE does not currenly work well with TSan
--source include/not_tsan.inc
--source include/no_valgrind_without_big.inc
CALL mtr.add_suppression("==[0-9]*== Warning: set address range perms: large range");

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--source include/subquery.inc

set optimizer_switch=default;

