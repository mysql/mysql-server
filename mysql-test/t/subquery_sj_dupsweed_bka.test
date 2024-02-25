# 
# Run subquery_sj_dupsweed.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,mrr_cost_based=off';

--source t/subquery_sj_dupsweed.test

set optimizer_switch=default;
