SET @save_binlog_transaction_dependency_history_size= @@global.binlog_transaction_dependency_history_size;
SELECT COUNT(@@GLOBAL.binlog_transaction_dependency_history_size);
COUNT(@@GLOBAL.binlog_transaction_dependency_history_size)
1
SELECT COUNT(@@SESSION.binlog_transaction_dependency_history_size);
ERROR HY000: Variable 'binlog_transaction_dependency_history_size' is a GLOBAL variable
SELECT VARIABLE_NAME FROM performance_schema.global_variables WHERE VARIABLE_NAME='binlog_transaction_dependency_history_size';
VARIABLE_NAME
binlog_transaction_dependency_history_size
SELECT VARIABLE_NAME FROM performance_schema.session_variables WHERE variable_name='binlog_transaction_dependency_history_size';
VARIABLE_NAME
binlog_transaction_dependency_history_size
SET @@GLOBAL.binlog_transaction_dependency_history_size= 1;
include/assert.inc ['binlog_transaction_dependency_history_size is a dynamic variable']
SET @@GLOBAL.binlog_transaction_dependency_history_size= default;
include/assert.inc ['Default value is 25000']
SET @@GLOBAL.binlog_transaction_dependency_history_size= ON;
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_history_size'
SET @@GLOBAL.binlog_transaction_dependency_history_size= OFF;
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_history_size'
SET @@GLOBAL.binlog_transaction_dependency_history_size= "foo";
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_history_size'
SET @@GLOBAL.binlog_transaction_dependency_history_size= NULL;
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_history_size'
SET @@GLOBAL.binlog_transaction_dependency_history_size= 1.0;
ERROR 42000: Incorrect argument type to variable 'binlog_transaction_dependency_history_size'
Expect value still set to 25000
SELECT @@GLOBAL.binlog_transaction_dependency_history_size;
@@GLOBAL.binlog_transaction_dependency_history_size
25000
SET @@GLOBAL.binlog_transaction_dependency_history_size= 0;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_dependency_history_size value: '0'
SET @@GLOBAL.binlog_transaction_dependency_history_size= -1;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_dependency_history_size value: '-1'
SET @@GLOBAL.binlog_transaction_dependency_history_size= 2000000;
Warnings:
Warning	1292	Truncated incorrect binlog_transaction_dependency_history_size value: '2000000'
SET @@GLOBAL.binlog_transaction_dependency_history_size= 1;
SET @@GLOBAL.binlog_transaction_dependency_history_size= 20000;
SET @@GLOBAL.binlog_transaction_dependency_history_size= 1000000;
SET @@GLOBAL.binlog_transaction_dependency_history_size= @save_binlog_transaction_dependency_history_size;
