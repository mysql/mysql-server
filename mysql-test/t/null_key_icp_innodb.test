set optimizer_switch='index_condition_pushdown=on';

--disable_query_log
if (`select locate('semijoin', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='semijoin=off';
}
if (`select locate('materialization', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='materialization=off';
}
if (`select locate('mrr', @@optimizer_switch) > 0`) 
{
  set optimizer_switch='mrr=off';
}
--enable_query_log

--source include/null_key_innodb.inc

set optimizer_switch=default;

