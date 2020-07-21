#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
# 
# Run select_none.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,mrr_cost_based=off';

--source t/select_none.test

set optimizer_switch=default;
