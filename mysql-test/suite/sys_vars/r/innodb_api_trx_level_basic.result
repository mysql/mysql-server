SET @start_global_value = @@global.innodb_api_trx_level;
SELECT @start_global_value;
@start_global_value
0
Valid values are zero or above
SELECT @@global.innodb_api_trx_level >=0;
@@global.innodb_api_trx_level >=0
1
SELECT @@global.innodb_api_trx_level <=3;
@@global.innodb_api_trx_level <=3
1
SELECT @@global.innodb_api_trx_level;
@@global.innodb_api_trx_level
0
SELECT @@session.innodb_api_trx_level;
ERROR HY000: Variable 'innodb_api_trx_level' is a GLOBAL variable
SHOW global variables LIKE 'innodb_api_trx_level';
Variable_name	Value
innodb_api_trx_level	0
SHOW session variables LIKE 'innodb_api_trx_level';
Variable_name	Value
innodb_api_trx_level	0
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_api_trx_level';
VARIABLE_NAME	VARIABLE_VALUE
innodb_api_trx_level	0
SELECT * FROM performance_schema.session_variables 
WHERE variable_name='innodb_api_trx_level';
VARIABLE_NAME	VARIABLE_VALUE
innodb_api_trx_level	0
SET global innodb_api_trx_level=100;
Warnings:
Warning	1292	Truncated incorrect innodb_api_trx_level value: '100'
SELECT @@global.innodb_api_trx_level;
@@global.innodb_api_trx_level
3
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_api_trx_level';
VARIABLE_NAME	VARIABLE_VALUE
innodb_api_trx_level	3
SELECT * FROM performance_schema.session_variables 
WHERE variable_name='innodb_api_trx_level';
VARIABLE_NAME	VARIABLE_VALUE
innodb_api_trx_level	3
SET session innodb_api_trx_level=1;
ERROR HY000: Variable 'innodb_api_trx_level' is a GLOBAL variable and should be set with SET GLOBAL
SET global innodb_api_trx_level=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_api_trx_level'
SET global innodb_api_trx_level=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_api_trx_level'
SET global innodb_api_trx_level="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_api_trx_level'
SET global innodb_api_trx_level=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_api_trx_level value: '-7'
SELECT @@global.innodb_api_trx_level;
@@global.innodb_api_trx_level
0
SELECT * FROM performance_schema.global_variables 
WHERE variable_name='innodb_api_trx_level';
VARIABLE_NAME	VARIABLE_VALUE
innodb_api_trx_level	0
SET @@global.innodb_api_trx_level = @start_global_value;
SELECT @@global.innodb_api_trx_level;
@@global.innodb_api_trx_level
0
