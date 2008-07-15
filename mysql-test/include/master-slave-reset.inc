# Reset the master and the slave to start fresh.
#
# It is necessary to execute RESET MASTER and RESET SLAVE on both
# master and slave since the replication setup might be circular.
#
# Since we expect STOP SLAVE to produce a warning as the slave is
# stopped (the server was started with skip-slave-start), we disable
# warnings when doing STOP SLAVE.

connection slave;
--disable_warnings
stop slave;
source include/wait_for_slave_to_stop.inc;
--enable_warnings
connection master;
--disable_warnings
--disable_query_log
use test;
--enable_query_log
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
reset master;
--disable_query_log
reset slave;
--enable_query_log
connection slave;
reset slave;
# Clean up old test tables
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings
--disable_query_log
reset master;
--enable_query_log
start slave;
source include/wait_for_slave_to_start.inc;
