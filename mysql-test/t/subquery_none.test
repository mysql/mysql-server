# ER_STACK_OVERRUN_NEED_MORE does not currenly work well with TSan
--source include/not_tsan.inc

--source include/no_valgrind_without_big.inc
CALL mtr.add_suppression("==[0-9]*== Warning: set address range perms: large range");

# 
# Run subquery.inc without any of the socalled 6.0 features.
#

--disable_query_log
if (`select locate('semijoin', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='semijoin=off';
}
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
if (`select locate('index_condition_pushdown', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='index_condition_pushdown=off';
}
if (`select locate('mrr', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='mrr=off';
}
--enable_query_log

--source include/subquery.inc

set optimizer_switch=default;

