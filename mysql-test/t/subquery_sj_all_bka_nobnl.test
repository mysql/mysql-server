# 
# Run subquery_sj_all.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,block_nested_loop=off,mrr_cost_based=off';

--source t/subquery_sj_all.test

set optimizer_switch=default;
