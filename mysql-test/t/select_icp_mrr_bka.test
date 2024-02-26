#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
# 
# Run select_icp_mrr.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,mrr_cost_based=off';

--source t/select_icp_mrr.test

set optimizer_switch=default;
