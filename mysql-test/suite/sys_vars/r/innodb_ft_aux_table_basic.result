SET @start_global_value = @@global.innodb_ft_aux_table;
SELECT @start_global_value;
@start_global_value
NULL
select @@session.innodb_ft_aux_table;
ERROR HY000: Variable 'innodb_ft_aux_table' is a GLOBAL variable
show global variables like 'innodb_ft_aux_table';
Variable_name	Value
innodb_ft_aux_table	
show session variables like 'innodb_ft_aux_table';
Variable_name	Value
innodb_ft_aux_table	
select * from performance_schema.global_variables where variable_name='innodb_ft_aux_table';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_aux_table	
select * from performance_schema.session_variables where variable_name='innodb_ft_aux_table';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_aux_table	
set session innodb_ft_aux_table='Salmon';
ERROR HY000: Variable 'innodb_ft_aux_table' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_ft_aux_table='Salmon';
ERROR HY000: Variable 'innodb_ft_aux_table' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_ft_aux_table=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_aux_table'
set global innodb_ft_aux_table=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_ft_aux_table'
set global innodb_ft_aux_table='Salmon';
ERROR 42000: Variable 'innodb_ft_aux_table' can't be set to the value of 'Salmon'
SET @@global.innodb_ft_aux_table = @start_global_value;
SELECT @@global.innodb_ft_aux_table;
@@global.innodb_ft_aux_table
NULL
set @a:=repeat(':/',128);
set global innodb_ft_aux_table =@a;
ERROR 42000: Variable 'innodb_ft_aux_table' can't be set to the value of ':/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/'
set @a:=repeat(':/',256);
set global innodb_ft_aux_table =@a;
ERROR 42000: Variable 'innodb_ft_aux_table' can't be set to the value of ':/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/:/'
