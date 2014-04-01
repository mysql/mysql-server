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
eval CHANGE MASTER TO MASTER_USER='root', MASTER_CONNECT_RETRY=1, MASTER_HOST='127.0.0.1', MASTER_PORT=$MASTER_MYPORT;
START SLAVE;
source include/wait_for_slave_to_start.inc;

# Stop slave and reconnect to blackhole slave
connection slave;
STOP SLAVE;
RESET SLAVE;
eval CHANGE MASTER TO MASTER_USER='root', MASTER_CONNECT_RETRY=1, MASTER_HOST='127.0.0.1', MASTER_PORT=$BHS_MYPORT;
START SLAVE;
source include/wait_for_slave_to_start.inc;

--enable_query_log
--enable_warnings
