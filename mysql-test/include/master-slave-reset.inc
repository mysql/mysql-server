connection slave;
#we expect STOP SLAVE to produce a warning as the slave is stopped
#(the server was started with skip-slave-start)
--disable_warnings
stop slave;
--wait_for_slave_to_stop
--enable_warnings
connection master;
--disable_warnings
--disable_query_log
use test;
--enable_query_log
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
reset master;
connection slave;
reset slave;
# Clean up old test tables
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
start slave;
