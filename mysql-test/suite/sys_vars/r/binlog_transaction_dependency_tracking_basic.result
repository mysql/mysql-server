SET GLOBAL binlog_transaction_dependency_tracking= 'COMMIT_ORDER';
SET GLOBAL transaction_write_set_extraction= 'XXHASH64';
SELECT COUNT(@@GLOBAL.binlog_transaction_dependency_tracking);
COUNT(@@GLOBAL.binlog_transaction_dependency_tracking)
1
SELECT COUNT(@@SESSION.binlog_transaction_dependency_tracking);
ERROR HY000: Variable 'binlog_transaction_dependency_tracking' is a GLOBAL variable
SELECT VARIABLE_NAME FROM performance_schema.global_variables WHERE VARIABLE_NAME='binlog_transaction_dependency_tracking';
VARIABLE_NAME
binlog_transaction_dependency_tracking
SELECT VARIABLE_NAME FROM performance_schema.session_variables WHERE variable_name='binlog_transaction_dependency_tracking';
VARIABLE_NAME
binlog_transaction_dependency_tracking
SET GLOBAL binlog_transaction_dependency_tracking= 'COMMIT_ORDER';
include/assert.inc ['binlog_transaction_dependency_tracking is a dynamic variable']
SET GLOBAL binlog_transaction_dependency_tracking= 'COMMIT_ORDER';
include/assert.inc ['binlog_transaction_dependency_tracking should be COMMIT_ORDER']
SET GLOBAL binlog_transaction_dependency_tracking= 0;
include/assert.inc ['binlog_transaction_dependency_tracking should be COMMIT_ORDER']
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET';
include/assert.inc ['binlog_transaction_dependency_tracking should be WRITESET']
SET GLOBAL binlog_transaction_dependency_tracking= 1;
include/assert.inc ['binlog_transaction_dependency_tracking should be WRITESET']
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET_SESSION';
include/assert.inc ['binlog_transaction_dependency_tracking should be WRITESET_SESSION']
SET GLOBAL binlog_transaction_dependency_tracking= 2;
include/assert.inc ['binlog_transaction_dependency_tracking should be WRITESET_SESSION']
SET GLOBAL binlog_transaction_dependency_tracking= DEFAULT;
include/assert.inc ['binlog_transaction_dependency_tracking should be COMMIT_ORDER']
SET GLOBAL binlog_transaction_dependency_tracking= NULL;
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of 'NULL'
SET GLOBAL binlog_transaction_dependency_tracking= '';
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of ''
SET GLOBAL binlog_transaction_dependency_tracking= 1.0;
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_tracking'
SET GLOBAL binlog_transaction_dependency_tracking= 'GARBAGE';
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of 'GARBAGE'
SET GLOBAL binlog_transaction_dependency_tracking= ON;
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of 'ON'
SET GLOBAL binlog_transaction_dependency_tracking= OFF;
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of 'OFF'
SET GLOBAL binlog_transaction_dependency_tracking= 3;
ERROR 42000: Variable 'binlog_transaction_dependency_tracking' can't be set to the value of '3'
Expect value still set to "COMMIT_ORDER"
SELECT @@global.binlog_transaction_dependency_tracking;
@@global.binlog_transaction_dependency_tracking
COMMIT_ORDER
Case1 a): "When binlog_transaction_dependency_tracking != COMMIT_ORDER,
transaction_write_set_extraction cannot be changed."
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET';
SET GLOBAL transaction_write_set_extraction= OFF;
ERROR HY000: Incorrect usage of transaction_write_set_extraction (changed) and binlog_transaction_dependency_tracking (!= COMMIT_ORDER)
SET GLOBAL transaction_write_set_extraction= MURMUR32;
ERROR HY000: Incorrect usage of transaction_write_set_extraction (changed) and binlog_transaction_dependency_tracking (!= COMMIT_ORDER)
Expect value still set to XXHASH64
SELECT @@global.transaction_write_set_extraction;
@@global.transaction_write_set_extraction
XXHASH64
Case1 b): "When binlog_transaction_dependency_tracking != COMMIT_ORDER,
transaction_write_set_extraction cannot be changed."
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET_SESSION';
SET GLOBAL transaction_write_set_extraction= OFF;
ERROR HY000: Incorrect usage of transaction_write_set_extraction (changed) and binlog_transaction_dependency_tracking (!= COMMIT_ORDER)
SET GLOBAL transaction_write_set_extraction= MURMUR32;
ERROR HY000: Incorrect usage of transaction_write_set_extraction (changed) and binlog_transaction_dependency_tracking (!= COMMIT_ORDER)
Expect value still set to XXHASH64
SELECT @@global.transaction_write_set_extraction;
@@global.transaction_write_set_extraction
XXHASH64
Case2: "When transaction_write_set_extraction = OFF,
binlog_transaction_dependency_tracking cannot be set to WRITESET%"
SET GLOBAL binlog_transaction_dependency_tracking= 'COMMIT_ORDER';
SET GLOBAL transaction_write_set_extraction= OFF;
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET';
ERROR HY000: Incorrect usage of binlog_transaction_dependency_tracking (!= COMMIT_ORDER) and transaction_write_set_extraction (= OFF)
SET GLOBAL binlog_transaction_dependency_tracking= 'WRITESET_SESSION';
ERROR HY000: Incorrect usage of binlog_transaction_dependency_tracking (!= COMMIT_ORDER) and transaction_write_set_extraction (= OFF)
Expect value still set to COMMIT_ORDER
SELECT @@global.binlog_transaction_dependency_tracking;
@@global.binlog_transaction_dependency_tracking
COMMIT_ORDER
SET GLOBAL transaction_write_set_extraction= XXHASH64;
