SET @start_global_value = @@global.innodb_random_read_ahead;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF' 
select @@global.innodb_random_read_ahead in (0, 1);
@@global.innodb_random_read_ahead in (0, 1)
1
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
0
select @@session.innodb_random_read_ahead;
ERROR HY000: Variable 'innodb_random_read_ahead' is a GLOBAL variable
show global variables like 'innodb_random_read_ahead';
Variable_name	Value
innodb_random_read_ahead	OFF
show session variables like 'innodb_random_read_ahead';
Variable_name	Value
innodb_random_read_ahead	OFF
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
set global innodb_random_read_ahead='ON';
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
1
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
set @@global.innodb_random_read_ahead=0;
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
0
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
set global innodb_random_read_ahead=1;
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
1
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
set @@global.innodb_random_read_ahead='OFF';
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
0
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	OFF
set session innodb_random_read_ahead='OFF';
ERROR HY000: Variable 'innodb_random_read_ahead' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_random_read_ahead='ON';
ERROR HY000: Variable 'innodb_random_read_ahead' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_random_read_ahead=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_random_read_ahead'
set global innodb_random_read_ahead=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_random_read_ahead'
set global innodb_random_read_ahead=2;
ERROR 42000: Variable 'innodb_random_read_ahead' can't be set to the value of '2'
NOTE: The following should fail with ER_WRONG_VALUE_FOR_VAR (BUG#50643)
set global innodb_random_read_ahead=-3;
select @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
1
select * from performance_schema.global_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
select * from performance_schema.session_variables where variable_name='innodb_random_read_ahead';
VARIABLE_NAME	VARIABLE_VALUE
innodb_random_read_ahead	ON
set global innodb_random_read_ahead='AUTO';
ERROR 42000: Variable 'innodb_random_read_ahead' can't be set to the value of 'AUTO'
SET @@global.innodb_random_read_ahead = @start_global_value;
SELECT @@global.innodb_random_read_ahead;
@@global.innodb_random_read_ahead
0
