#############################################################
# Author:  Serge Kozlov <skozlov@mysql.com>
# Date:    03/17/2008
# Purpose: Set up circular cluster replication where each 
# cluster has two mysqlds and replication directions are 
# following:
#          master ---> slave  
#           /            \
#     cluster A        cluster B
#           \            /
#         master1 <--- slave1
#############################################################

--source include/have_log_bin.inc

# Make connections to mysqlds

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT1,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT1,);

# Check that all mysqld compiled with ndb support

--connection master
--disable_query_log
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT') AS `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
--enable_query_log

--connection master1
--disable_query_log
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT') AS `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
--enable_query_log

--connection slave
--disable_query_log
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT') AS `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
--enable_query_log

--connection slave1
--disable_query_log
--require r/true.require
SELECT (support = 'YES' or support = 'DEFAULT') AS `TRUE` FROM information_schema.engines WHERE engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
--enable_query_log

# Stop slaves

--connection master
--disable_warnings
STOP SLAVE;
--wait_for_slave_to_stop
--enable_warnings

--connection master1
--disable_warnings
STOP SLAVE;
--wait_for_slave_to_stop
--enable_warnings

--connection slave
--disable_warnings
STOP SLAVE;
--wait_for_slave_to_stop
--enable_warnings

--connection slave1
--disable_warnings
STOP SLAVE;
--wait_for_slave_to_stop
--enable_warnings

# Reset masters

--connection master
--disable_warnings
--disable_query_log
USE test;
--enable_query_log
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
RESET MASTER;

--connection master1
--disable_warnings
--disable_query_log
USE test;
--enable_query_log
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
RESET MASTER;

--connection slave
--disable_warnings
--disable_query_log
USE test;
--enable_query_log
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
RESET MASTER;

--connection slave1
--disable_warnings
--disable_query_log
USE test;
--enable_query_log
DROP TABLE IF EXISTS t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
RESET MASTER;

# Start slaves

--connection slave
RESET SLAVE;
--replace_result $MASTER_MYPORT MASTER_MYPORT
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$MASTER_MYPORT,master_user='root'
START SLAVE;
--source include/wait_for_slave_to_start.inc

--connection master1
RESET SLAVE;
--replace_result $SLAVE_MYPORT1 SLAVE_MYPORT1
--eval CHANGE MASTER TO master_host='127.0.0.1',master_port=$SLAVE_MYPORT1,master_user='root'
START SLAVE;
--source include/wait_for_slave_to_start.inc


# Set the default connection to 'master' (cluster A)
connection master;

