# This tests the functionality of the Myisam engine
# These testcases also exist in InnoDB engine
# All tests are required to run with Myisam
# Hence MTR starts mysqld with MyISAM as default

--source include/force_myisam_default.inc
--source include/have_myisam.inc

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on';

--source include/icp_tests.inc

set optimizer_switch=default;
