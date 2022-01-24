SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
0
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
0
SET @@global.global_connection_memory_tracking = ON;
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
1
SET @@session.global_connection_memory_tracking = ON;
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
1
SET @@global.global_connection_memory_tracking = -5666777;
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of '-5666777'
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
1
SET @@session.global_connection_memory_tracking = -5666777;
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of '-5666777'
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
1
SET @@global.global_connection_memory_tracking = -1;
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of '-1'
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
1
SET @@session.global_connection_memory_tracking = -1;
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of '-1'
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
1
SET @@global.global_connection_memory_tracking = OFF;
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
0
SET @@session.global_connection_memory_tracking = OFF;
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
0
SET @@global.global_connection_memory_tracking = default;
SET @@session.global_connection_memory_tracking = default;
SET @@global.global_connection_memory_tracking = NULL;
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of 'NULL'
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
0
SET @@global.global_connection_memory_tracking = 'foo';
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of 'foo'
SELECT @@global.global_connection_memory_tracking;
@@global.global_connection_memory_tracking
0
SET @@session.global_connection_memory_tracking = 'foo';
ERROR 42000: Variable 'global_connection_memory_tracking' can't be set to the value of 'foo'
SELECT @@session.global_connection_memory_tracking;
@@session.global_connection_memory_tracking
0
