--connection mysqld_a
--disable_query_log
--sync_slave_with_master mysqld_b
--sync_slave_with_master mysqld_c
--connection mysqld_a
--enable_query_log

