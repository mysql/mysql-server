# 
# Run subquery_mat.inc with subquery materialization
#

set optimizer_switch='materialization=on';

--disable_query_log
if (`select locate('semijoin', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='semijoin=off';
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

--source include/subquery_mat.inc

set optimizer_switch=default;
