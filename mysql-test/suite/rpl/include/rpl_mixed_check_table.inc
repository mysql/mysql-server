#########################################
# Author: Serge Kozlov skozlov@mysql.com
# Date: 07/10/2006
# Purpose: Checking created tables on master and same tables on slave 
# Requirements: 
#########################################

--echo ==========MASTER==========
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;
sync_slave_with_master;
--echo ==========SLAVE===========
USE test_rpl;
SHOW CREATE TABLE t1;
SHOW CREATE TABLE t2;
connection master;
