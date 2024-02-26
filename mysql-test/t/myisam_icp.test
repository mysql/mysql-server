# This tests the functionality of the Myisam engine
# These testcases also exist in InnoDB engine
# All tests are required to run with Myisam
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

#
# ICP/MyISAM tests (Index Condition Pushdown)
# (Turns off all other 6.0 optimizer switches)
#

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

--source include/icp_tests.inc

set optimizer_switch=default;
