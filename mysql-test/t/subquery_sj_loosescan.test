--source include/no_valgrind_without_big.inc

#
# Run subquery_sj.inc with semijoin and turn off all strategies, but LooseScan
#

set optimizer_switch='semijoin=on,loosescan=on';

--disable_query_log
if (`select locate('materialization', @@optimizer_switch) > 0`)
{
  set optimizer_switch='materialization=off';
}
if (`select locate('firstmatch', @@optimizer_switch) > 0`)
{
  set optimizer_switch='firstmatch=off';
}
if (`select locate('duplicateweedout', @@optimizer_switch) > 0`)
{
  set optimizer_switch='duplicateweedout=off';
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

--source include/subquery_sj.inc

set optimizer_switch=default;

