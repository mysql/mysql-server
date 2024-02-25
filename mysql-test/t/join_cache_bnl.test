set optimizer_switch='block_nested_loop=on';

if (`select locate('mrr_cost_based', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='mrr_cost_based=off';
}

--source include/join_cache.inc

set optimizer_switch = default;
