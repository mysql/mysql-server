Valid values are between 0 and 1024
SELECT @@global.innodb_sync_array_size between 0 and 1024;
@@global.innodb_sync_array_size between 0 and 1024
1
SELECT @@global.innodb_sync_array_size;
@@global.innodb_sync_array_size
1
SELECT @@session.innodb_sync_array_size;
ERROR HY000: Variable 'innodb_sync_array_size' is a GLOBAL variable
SHOW GLOBAL variables LIKE 'innodb_sync_array_size';
Variable_name	Value
innodb_sync_array_size	1
SHOW SESSION variables LIKE 'innodb_sync_array_size';
Variable_name	Value
innodb_sync_array_size	1
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_sync_array_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_sync_array_size	1
SELECT * FROM performance_schema.session_variables 
WHERE variable_name='innodb_sync_array_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_sync_array_size	1
SET GLOBAL innodb_sync_array_size=10;
ERROR HY000: Variable 'innodb_sync_array_size' is a read only variable
SET SESSION innodb_sync_array_size=10;
ERROR HY000: Variable 'innodb_sync_array_size' is a read only variable
SELECT @@global.innodb_sync_array_size;
@@global.innodb_sync_array_size
1
