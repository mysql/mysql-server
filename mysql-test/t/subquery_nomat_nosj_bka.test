# ER_STACK_OVERRUN_NEED_MORE does not currenly work well with TSan
--source include/not_tsan.inc

--source include/no_valgrind_without_big.inc
# 
# Run subquery_nomat_nosj.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,mrr_cost_based=off';

--source t/subquery_nomat_nosj.test

set optimizer_switch=default;
