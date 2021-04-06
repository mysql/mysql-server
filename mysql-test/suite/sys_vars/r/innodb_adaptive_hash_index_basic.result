SET @start_global_value = @@global.innodb_adaptive_hash_index;
SELECT @start_global_value;
@start_global_value
1
Valid values are 'ON' and 'OFF' 
select @@global.innodb_adaptive_hash_index in (0, 1);
@@global.innodb_adaptive_hash_index in (0, 1)
1
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
1
select @@session.innodb_adaptive_hash_index;
ERROR HY000: Variable 'innodb_adaptive_hash_index' is a GLOBAL variable
show global variables like 'innodb_adaptive_hash_index';
Variable_name	Value
innodb_adaptive_hash_index	ON
show session variables like 'innodb_adaptive_hash_index';
Variable_name	Value
innodb_adaptive_hash_index	ON
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
set global innodb_adaptive_hash_index='OFF';
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
0
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	OFF
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	OFF
set @@global.innodb_adaptive_hash_index=1;
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
1
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
set global innodb_adaptive_hash_index=0;
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
0
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	OFF
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	OFF
set @@global.innodb_adaptive_hash_index='ON';
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
1
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
set session innodb_adaptive_hash_index='OFF';
ERROR HY000: Variable 'innodb_adaptive_hash_index' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_adaptive_hash_index='ON';
ERROR HY000: Variable 'innodb_adaptive_hash_index' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_adaptive_hash_index=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_adaptive_hash_index'
set global innodb_adaptive_hash_index=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_adaptive_hash_index'
set global innodb_adaptive_hash_index=2;
ERROR 42000: Variable 'innodb_adaptive_hash_index' can't be set to the value of '2'
set global innodb_adaptive_hash_index=-3;
ERROR 42000: Variable 'innodb_adaptive_hash_index' can't be set to the value of '-3'
select @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
1
select * from performance_schema.global_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
select * from performance_schema.session_variables where variable_name='innodb_adaptive_hash_index';
VARIABLE_NAME	VARIABLE_VALUE
innodb_adaptive_hash_index	ON
set global innodb_adaptive_hash_index='AUTO';
ERROR 42000: Variable 'innodb_adaptive_hash_index' can't be set to the value of 'AUTO'
SET @@global.innodb_adaptive_hash_index = @start_global_value;
SELECT @@global.innodb_adaptive_hash_index;
@@global.innodb_adaptive_hash_index
1
