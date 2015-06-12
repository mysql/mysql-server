#
# Basic test for innodb_sync_debug
#
SELECT @@global.innodb_sync_debug;
@@global.innodb_sync_debug
0
set global innodb_sync_debug = 1;
ERROR HY000: Variable 'innodb_sync_debug' is a read only variable
SELECT @@global.innodb_sync_debug;
@@global.innodb_sync_debug
0
