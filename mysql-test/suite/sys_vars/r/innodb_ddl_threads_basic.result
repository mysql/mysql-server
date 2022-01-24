SELECT @@global.innodb_ddl_threads;
@@global.innodb_ddl_threads
4
SELECT @@session.innodb_ddl_threads;
@@session.innodb_ddl_threads
4
SHOW GLOBAL VARIABLES LIKE  'innodb_ddl_threads';
Variable_name	Value
innodb_ddl_threads	4
SHOW SESSION VARIABLES LIKE 'innodb_ddl_threads';
Variable_name	Value
innodb_ddl_threads	4
SELECT * FROM performance_schema.global_variables
WHERE variable_name='innodb_ddl_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ddl_threads	4
SELECT * FROM performance_schema.session_variables
WHERE variable_name='innodb_ddl_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ddl_threads	4
set global innodb_ddl_threads=8;
set session innodb_ddl_threads=8;
set global innodb_ddl_threads=4;
set session innodb_ddl_threads=4;
