#Want to skip this test from daily Valgrind execution
--source include/no_valgrind_without_big.inc
# 
# Run select_all.test with BKA enabled 
#
set optimizer_switch='batched_key_access=on,block_nested_loop=off,mrr_cost_based=off';

--source t/select_all.test

set optimizer_switch=default;
