#
# Basic test for innodb_buffer_pool_debug
#
SELECT @@global.innodb_buffer_pool_debug;
@@global.innodb_buffer_pool_debug
0
set global innodb_buffer_pool_debug = 1;
ERROR HY000: Variable 'innodb_buffer_pool_debug' is a read only variable
SELECT @@global.innodb_buffer_pool_debug;
@@global.innodb_buffer_pool_debug
0
