SET @start_global_value = @@global.innodb_ft_enable_diag_print;
SELECT @start_global_value;
@start_global_value
0
Valid values are 'ON' and 'OFF' 
select @@global.innodb_ft_enable_diag_print in (0, 1);
@@global.innodb_ft_enable_diag_print in (0, 1)
1
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
0
select @@session.innodb_ft_enable_diag_print;
ERROR HY000: Variable 'innodb_ft_enable_diag_print' is a GLOBAL variable
show global variables like 'innodb_ft_enable_diag_print';
Variable_name	Value
innodb_ft_enable_diag_print	OFF
show session variables like 'innodb_ft_enable_diag_print';
Variable_name	Value
innodb_ft_enable_diag_print	OFF
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
set global innodb_ft_enable_diag_print='OFF';
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
0
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
set @@global.innodb_ft_enable_diag_print=1;
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
1
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
set global innodb_ft_enable_diag_print=0;
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
0
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	OFF
set @@global.innodb_ft_enable_diag_print='ON';
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
1
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
set session innodb_ft_enable_diag_print='OFF';
ERROR HY000: Variable 'innodb_ft_enable_diag_print' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_ft_enable_diag_print='ON';
ERROR HY000: Variable 'innodb_ft_enable_diag_print' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_ft_enable_diag_print=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_enable_diag_print'
set global innodb_ft_enable_diag_print=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_enable_diag_print'
set global innodb_ft_enable_diag_print=2;
ERROR 42000: Variable 'innodb_ft_enable_diag_print' can't be set to the value of '2'
NOTE: The following should fail with ER_WRONG_VALUE_FOR_VAR (BUG#50643)
set global innodb_ft_enable_diag_print=-3;
select @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
1
select * from performance_schema.global_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
select * from performance_schema.session_variables where variable_name='innodb_ft_enable_diag_print';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_enable_diag_print	ON
set global innodb_ft_enable_diag_print='AUTO';
ERROR 42000: Variable 'innodb_ft_enable_diag_print' can't be set to the value of 'AUTO'
SET @@global.innodb_ft_enable_diag_print = @start_global_value;
SELECT @@global.innodb_ft_enable_diag_print;
@@global.innodb_ft_enable_diag_print
0
