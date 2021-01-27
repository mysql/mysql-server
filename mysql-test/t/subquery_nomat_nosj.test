# ER_STACK_OVERRUN_NEED_MORE does not currenly work well with TSan
--source include/not_tsan.inc

--source include/no_valgrind_without_big.inc
CALL mtr.add_suppression("==[0-9]*== Warning: set address range perms: large range");

# 
# Run subquery.inc without semi-join optimization and subquery materialization
#

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

--source include/subquery.inc

set optimizer_switch=default;

