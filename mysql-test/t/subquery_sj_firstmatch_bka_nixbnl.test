# 
# Run subquery_sj_firstmatch.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,block_nested_loop=off,mrr_cost_based=off';

--source t/subquery_sj_firstmatch.test

set optimizer_switch=default;
