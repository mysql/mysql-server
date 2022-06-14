# Insert blackhole slave into regular replication chain
# We hide any output below due using untouched result files of rpl suite

--disable_warnings
--disable_query_log

connection master;

# Connect blackhole slave to master.
connect (blackhole_slave,127.0.0.1,root,,test,$BHS_MYPORT,);
connection blackhole_slave;
source include/have_blackhole.inc;
SET default_storage_engine=BLACKHOLE;
STOP SLAVE;
source include/wait_for_slave_to_stop.inc;
RESET SLAVE;
eval CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_CONNECT_RETRY=1, SOURCE_HOST='127.0.0.1', SOURCE_PORT=$MASTER_MYPORT;
START SLAVE;
source include/wait_for_slave_to_start.inc;

# Stop slave and reconnect to blackhole slave
connection slave;
STOP SLAVE;
RESET SLAVE;
eval CHANGE REPLICATION SOURCE TO SOURCE_USER='root', SOURCE_CONNECT_RETRY=1, SOURCE_HOST='127.0.0.1', SOURCE_PORT=$BHS_MYPORT;
START SLAVE;
source include/wait_for_slave_to_start.inc;

--enable_query_log
--enable_warnings
