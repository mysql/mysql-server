#
# Bug#33038262: VARIOUS RACES IN PERSISTEN SYSTEM VARIABLE IMPLEMENTATION
#
SET @@debug='+d,check_intern_find_sys_var_lock';
SET PERSIST_ONLY sql_mode=DEFAULT;
SET PERSIST sql_mode=DEFAULT;
RESET PERSIST sql_mode;
RESET PERSIST;
SET @@debug='-d,check_intern_find_sys_var_lock';
# cleanup
# restart
#
# Bug #33830493: SET PERSIST statement is not atomic
#
SET PERSIST max_connections=42;
SET @@debug='+d,crash_after_open_persist_file';
SET PERSIST max_heap_table_size=887808, replica_net_timeout=160;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT @@max_connections, @@max_heap_table_size, @@replica_net_timeout;
@@max_connections	@@max_heap_table_size	@@replica_net_timeout
42	16777216	60
SELECT variable_name FROM performance_schema.variables_info WHERE variable_source='PERSISTED';
variable_name
max_connections
SET @@debug='-d,crash_after_open_persist_file';
SET @@debug='+d,crash_after_write_persist_file';
SET PERSIST replica_net_timeout=160;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT @@max_connections, @@max_heap_table_size, @@replica_net_timeout;
@@max_connections	@@max_heap_table_size	@@replica_net_timeout
42	16777216	60
SELECT variable_name FROM performance_schema.variables_info WHERE variable_source='PERSISTED';
variable_name
max_connections
SET @@debug='-d,crash_after_write_persist_file';
SET @@debug='+d,crash_after_close_persist_file';
SET PERSIST replica_net_timeout=124;
ERROR HY000: Lost connection to MySQL server during query
# restart
SELECT @@max_connections, @@max_heap_table_size, @@replica_net_timeout;
@@max_connections	@@max_heap_table_size	@@replica_net_timeout
42	16777216	124
SELECT variable_name FROM performance_schema.variables_info WHERE variable_source='PERSISTED';
variable_name
max_connections
replica_net_timeout
slave_net_timeout
SET @@debug='-d,crash_after_close_persist_file';
RESET PERSIST;
SET GLOBAL max_connections = default, replica_net_timeout = default, max_heap_table_size = default;
# restart:
