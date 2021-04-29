SET @start_global_value = @@global.innodb_strict_mode;
SELECT @start_global_value;
@start_global_value
1
Valid values are 'ON' and 'OFF' 
select @@global.innodb_strict_mode in (0, 1);
@@global.innodb_strict_mode in (0, 1)
1
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
1
select @@session.innodb_strict_mode in (0, 1);
@@session.innodb_strict_mode in (0, 1)
1
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
show global variables like 'innodb_strict_mode';
Variable_name	Value
innodb_strict_mode	ON
show session variables like 'innodb_strict_mode';
Variable_name	Value
innodb_strict_mode	ON
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
set global innodb_strict_mode='OFF';
set session innodb_strict_mode='OFF';
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
0
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
0
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	OFF
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	OFF
set @@global.innodb_strict_mode=1;
set @@session.innodb_strict_mode=1;
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
1
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
set global innodb_strict_mode=0;
set session innodb_strict_mode=0;
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
0
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
0
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	OFF
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	OFF
set @@global.innodb_strict_mode='ON';
set @@session.innodb_strict_mode='ON';
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
1
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
set global innodb_strict_mode=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_strict_mode'
set session innodb_strict_mode=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_strict_mode'
set global innodb_strict_mode=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_strict_mode'
set session innodb_strict_mode=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_strict_mode'
set global innodb_strict_mode=2;
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of '2'
set session innodb_strict_mode=2;
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of '2'
set global innodb_strict_mode='AUTO';
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of 'AUTO'
set session innodb_strict_mode='AUTO';
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of 'AUTO'
set global innodb_strict_mode=-3;
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of '-3'
set session innodb_strict_mode=-7;
ERROR 42000: Variable 'innodb_strict_mode' can't be set to the value of '-7'
select @@global.innodb_strict_mode;
@@global.innodb_strict_mode
1
select @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
select * from performance_schema.global_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
select * from performance_schema.session_variables where variable_name='innodb_strict_mode';
VARIABLE_NAME	VARIABLE_VALUE
innodb_strict_mode	ON
CREATE USER foo, bar;
GRANT SESSION_VARIABLES_ADMIN ON *.*  TO bar;
SELECT @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
SET @@session.innodb_strict_mode = 'OFF';
ERROR 42000: Access denied; you need (at least one of) the SYSTEM_VARIABLES_ADMIN or SESSION_VARIABLES_ADMIN privilege(s) for this operation
SELECT @@session.innodb_strict_mode;
@@session.innodb_strict_mode
1
SET @@session.innodb_strict_mode = 'OFF';
SET @@session.innodb_strict_mode = 1;
SET @@session.innodb_strict_mode = FALSE;
SELECT @@session.innodb_strict_mode;
@@session.innodb_strict_mode
0
SET @@global.innodb_strict_mode = FALSE;
ERROR 42000: Access denied; you need (at least one of) the SUPER or SYSTEM_VARIABLES_ADMIN privilege(s) for this operation
SET @@session.innodb_table_locks = -1;
ERROR 42000: Variable 'innodb_table_locks' can't be set to the value of '-1'
SET @@session.innodb_table_locks = 'STR';
ERROR 42000: Variable 'innodb_table_locks' can't be set to the value of 'STR'
DROP USER foo, bar;
SET @@global.innodb_strict_mode = @start_global_value;
SELECT @@global.innodb_strict_mode;
@@global.innodb_strict_mode
1
