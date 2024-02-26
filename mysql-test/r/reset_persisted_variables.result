#
# WL#9763: Support option to RESET PERSIST
#
# Syntax check for RESET PERSIST
RESET PERSIST;
RESET PERSIST max_connections;
ERROR HY000: Variable max_connections does not exist in persisted config file
RESET PERSIST IF EXISTS max_connections;
Warnings:
Warning	3615	Variable max_connections does not exist in persisted config file
RESET PERSIST IF EXISTS;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1
RESET PERSIST IF EXISTS max_connections, long_query_time;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ', long_query_time' at line 1
RESET PERSIST max_connections, long_query_time;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ', long_query_time' at line 1
# check contents of persistent config file
SET PERSIST sort_buffer_size=256000;
SET PERSIST long_query_time= 8.3452;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
long_query_time	8.345200
sort_buffer_size	256000
RESET PERSIST;
# config file should be empty
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
# add few more to config file and reset
SET PERSIST sort_buffer_size=156000,max_connections= 52;
SET PERSIST max_heap_table_size=887808, replica_net_timeout=160;
SET PERSIST long_query_time= 7.8102;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
long_query_time	7.810200
max_connections	52
max_heap_table_size	887808
replica_net_timeout	160
slave_net_timeout	160
sort_buffer_size	156000
RESET PERSIST max_heap_table_size;
RESET PERSIST sort_buffer_size;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
long_query_time	7.810200
max_connections	52
replica_net_timeout	160
slave_net_timeout	160
RESET PERSIST IF EXISTS replica_net_timeout;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
long_query_time	7.810200
max_connections	52
SET PERSIST join_buffer_size= 262144;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
join_buffer_size	262144
long_query_time	7.810200
max_connections	52
RESET PERSIST;
select * from performance_schema.persisted_variables ORDER BY 1;
VARIABLE_NAME	VARIABLE_VALUE
# check for errors and warnings
RESET PERSIST IF EXISTS sort_buffer_size;
Warnings:
Warning	3615	Variable sort_buffer_size does not exist in persisted config file
RESET PERSIST sort_buffer_size;
ERROR HY000: Variable sort_buffer_size does not exist in persisted config file
SET GLOBAL long_query_time= DEFAULT,
max_connections= DEFAULT, max_heap_table_size= DEFAULT,
replica_net_timeout= DEFAULT, sort_buffer_size= DEFAULT,
join_buffer_size= DEFAULT;
#
# Bug #27264789: CONCURRENT EXECUTION OF SET/RESET PERSIST CALL CRASHES SERVER
#
SET DEBUG_SYNC='in_set_persist_variables SIGNAL set WAIT_FOR go';
SET PERSIST max_connections= 38;;
SET DEBUG_SYNC='now WAIT_FOR set';
RESET PERSIST IF EXISTS max_connections;;
SET DEBUG_SYNC='now SIGNAL go';
SELECT count(*) FROM performance_schema.persisted_variables
WHERE variable_name = 'max_connections';
count(*)
0
SET DEBUG_SYNC='RESET';
SET GLOBAL max_connections=DEFAULT;
