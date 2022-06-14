--source include/no_valgrind_without_big.inc

set optimizer_switch='batched_key_access=on,block_nested_loop=off,mrr_cost_based=off';

--source include/join_cache.inc

set optimizer_switch = default;
