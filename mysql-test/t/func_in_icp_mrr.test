--source include/no_valgrind_without_big.inc

set optimizer_switch='index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--disable_query_log
if (`select locate('semijoin', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='semijoin=off';
}
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
--enable_query_log

--source include/func_in.inc

set optimizer_switch=default;

