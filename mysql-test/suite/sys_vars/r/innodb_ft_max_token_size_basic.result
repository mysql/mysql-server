select @@global.innodb_ft_max_token_size;
@@global.innodb_ft_max_token_size
84
select @@session.innodb_ft_max_token_size;
ERROR HY000: Variable 'innodb_ft_max_token_size' is a GLOBAL variable
show global variables like 'innodb_ft_max_token_size';
Variable_name	Value
innodb_ft_max_token_size	84
show session variables like 'innodb_ft_max_token_size';
Variable_name	Value
innodb_ft_max_token_size	84
select * from performance_schema.global_variables where variable_name='innodb_ft_max_token_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_max_token_size	84
select * from performance_schema.session_variables where variable_name='innodb_ft_max_token_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_ft_max_token_size	84
set global innodb_ft_max_token_size=1;
ERROR HY000: Variable 'innodb_ft_max_token_size' is a read only variable
set session innodb_ft_max_token_size=1;
ERROR HY000: Variable 'innodb_ft_max_token_size' is a read only variable
