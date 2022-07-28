#
# WL#13469: secure storage for sensitive system variables
#
# Copy Pre-8.0.28 format persisted variables file to data directory
# Restart the server
# Verify that persisted variable file was read properly: Should show 5 entries
SELECT * FROM performance_schema.persisted_variables ORDER BY variable_name ASC;
VARIABLE_NAME	VARIABLE_VALUE
auto_increment_increment	20
back_log	100
net_buffer_length	32768
partial_revokes	ON
performance_schema_error_size	5000
# Check the actual values
SELECT @@global.partial_revokes;
@@global.partial_revokes
1
SELECT @@global.auto_increment_increment;
@@global.auto_increment_increment
20
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
32768
SELECT @@global.back_log;
@@global.back_log
100
# Modify persisted variables
SET PERSIST_ONLY partial_revokes=OFF;
SET PERSIST_ONLY auto_increment_increment=30;
SET PERSIST_ONLY net_buffer_length=8192;
SET PERSIST_ONLY back_log=200;
SET PERSIST_ONLY performance_schema_error_size=10000;
# Restart the server
# Verify that persisted variable file was read properly: Should show 5 entries
SELECT * FROM performance_schema.persisted_variables ORDER BY variable_name ASC;
VARIABLE_NAME	VARIABLE_VALUE
auto_increment_increment	30
back_log	200
net_buffer_length	8192
partial_revokes	OFF
performance_schema_error_size	10000
# Check the actual values
SELECT @@global.partial_revokes;
@@global.partial_revokes
0
SELECT @@global.auto_increment_increment;
@@global.auto_increment_increment
30
SELECT @@global.net_buffer_length;
@@global.net_buffer_length
8192
SELECT @@global.back_log;
@@global.back_log
200
RESET PERSIST;
# Remove old format persisted variables file
# Cleanup: Restart with default options.
