SET @start_global_value = @@global.innodb_ft_num_word_optimize;
SELECT @start_global_value;
@start_global_value
2000
Valid values are zero or above
select @@global.innodb_ft_num_word_optimize >=1000;
@@global.innodb_ft_num_word_optimize >=1000
1
select @@global.innodb_ft_num_word_optimize;
@@global.innodb_ft_num_word_optimize
2000
select @@session.innodb_ft_num_word_optimize;
ERROR HY000: Variable 'innodb_ft_num_word_optimize' is a GLOBAL variable
show global variables like 'innodb_ft_num_word_optimize';
Variable_name	Value
innodb_ft_num_word_optimize	2000
show session variables like 'innodb_ft_num_word_optimize';
Variable_name	Value
innodb_ft_num_word_optimize	2000
select * from performance_schema.global_variables where variable_name='innodb_ft_num_word_optimize';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_num_word_optimize	2000
select * from performance_schema.session_variables where variable_name='innodb_ft_num_word_optimize';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_num_word_optimize	2000
set global innodb_ft_num_word_optimize=1000;
select @@global.innodb_ft_num_word_optimize;
@@global.innodb_ft_num_word_optimize
1000
select * from performance_schema.global_variables where variable_name='innodb_ft_num_word_optimize';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_num_word_optimize	1000
select * from performance_schema.session_variables where variable_name='innodb_ft_num_word_optimize';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_num_word_optimize	1000
set session innodb_ft_num_word_optimize=1000;
ERROR HY000: Variable 'innodb_ft_num_word_optimize' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_ft_num_word_optimize=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_num_word_optimize'
set global innodb_ft_num_word_optimize=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_num_word_optimize'
set global innodb_ft_num_word_optimize="foo";
ERROR 42000: Incorrect argument type to variable 'innodb_ft_num_word_optimize'
set global innodb_ft_num_word_optimize=-7;
Warnings:
Warning	1292	Truncated incorrect innodb_ft_num_word_optimize value: '-7'
select @@global.innodb_ft_num_word_optimize;
@@global.innodb_ft_num_word_optimize
1000
select * from performance_schema.global_variables where variable_name='innodb_ft_num_word_optimize';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_num_word_optimize	1000
SET @@global.innodb_ft_num_word_optimize = @start_global_value;
SELECT @@global.innodb_ft_num_word_optimize;
@@global.innodb_ft_num_word_optimize
2000
