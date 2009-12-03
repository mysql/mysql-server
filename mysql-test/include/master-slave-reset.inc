# Reset the master and the slave to start fresh.
#
# It is necessary to execute RESET MASTER and RESET SLAVE on both
# master and slave since the replication setup might be circular.
#
# Since we expect STOP SLAVE to produce a warning as the slave is
# stopped (the server was started with skip-slave-start), we disable
# warnings when doing STOP SLAVE.
#
# $no_change_master  If true, no change master will be done nor any reset slave.
#                    This is to avoid touching the relay-log.info file allowing
#                    the test to create one itself.
# $skip_slave_start  If true, the slave will not be started

connection slave;

#we expect STOP SLAVE to produce a warning as the slave is stopped
#(the server was started with skip-slave-start)
--disable_warnings
stop slave;
--disable_query_log
if (!$no_change_master) {
  eval CHANGE MASTER TO MASTER_USER='root',
                        MASTER_CONNECT_RETRY=1,
                        MASTER_HOST='127.0.0.1',
                        MASTER_PORT=$MASTER_MYPORT;
}
--enable_query_log
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
if (!$no_change_master) {
  reset slave;
}
--enable_query_log
connection slave;

--disable_warnings
# the first RESET SLAVE may produce a warning about non-existent
# 'ndb_apply_status' table, because this table is created
# asynchronously at the server startup and may not exist yet
# if RESET SLAVE comes too soon after the server startup
if (!$no_change_master) {
  reset slave;
}
--enable_warnings

# Clean up old test tables
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
--enable_warnings

--disable_query_log
#eval CHANGE MASTER TO MASTER_USER='root',
#                      MASTER_CONNECT_RETRY=1,
#                      MASTER_HOST='127.0.0.1',
#                      MASTER_PORT=$MASTER_MYPORT;
reset master;
--enable_query_log

if (!$skip_slave_start) {
  start slave;
  source include/wait_for_slave_to_start.inc;
}

