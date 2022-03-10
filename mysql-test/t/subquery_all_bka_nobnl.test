# ER_STACK_OVERRUN_NEED_MORE does not currenly work well with TSan
--source include/not_tsan.inc

# 
# Run subquery_all.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,block_nested_loop=off,mrr_cost_based=off';

--source t/subquery_all.test

set optimizer_switch=default;
