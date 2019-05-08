SELECT @@global.innodb_buffer_pool_in_core_file;
@@global.innodb_buffer_pool_in_core_file
1
SELECT @@session.innodb_buffer_pool_in_core_file;
ERROR HY000: Variable 'innodb_buffer_pool_in_core_file' is a GLOBAL variable
SHOW GLOBAL VARIABLES LIKE 'innodb_buffer_pool_in_core_file';
Variable_name	Value
innodb_buffer_pool_in_core_file	ON
SHOW SESSION VARIABLES LIKE 'innodb_buffer_pool_in_core_file';
Variable_name	Value
innodb_buffer_pool_in_core_file	ON
SELECT * FROM performance_schema.global_variables
WHERE variable_name='innodb_buffer_pool_in_core_file';
VARIABLE_NAME	VARIABLE_VALUE
innodb_buffer_pool_in_core_file	ON
SELECT * FROM performance_schema.session_variables
WHERE variable_name='innodb_buffer_pool_in_core_file';
VARIABLE_NAME	VARIABLE_VALUE
innodb_buffer_pool_in_core_file	ON
SET GLOBAL innodb_buffer_pool_in_core_file = default;
SET GLOBAL innodb_buffer_pool_in_core_file = true;
