--connection master
--sync_slave_with_master
--connection slave
--disable_query_log
STOP SLAVE;
--enable_query_log
