#########################################
# Author: Serge Kozlov skozlov@mysql.com
# Date: 07/10/2006
# Purpose: Showing data from work tables on master and on slave
# Requirements: 
#########################################

--echo ==========MASTER==========
SELECT COUNT(*) FROM t1;
SELECT * FROM t1 ORDER BY a;
SELECT COUNT(*) FROM t2;
SELECT * FROM t2 ORDER BY a;
sync_slave_with_master;
--echo ==========SLAVE===========
USE test_rpl;
SELECT COUNT(*) FROM t1;
SELECT * FROM t1 ORDER BY a;
SELECT COUNT(*) FROM t2;
SELECT * FROM t2 ORDER BY a;
connection master;

