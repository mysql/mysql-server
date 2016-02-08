select @@global.rand_seed2;
ERROR HY000: Variable 'rand_seed2' is a SESSION variable
select @@session.rand_seed2;
@@session.rand_seed2
0
show global variables like 'rand_seed2';
Variable_name	Value
show session variables like 'rand_seed2';
Variable_name	Value
rand_seed2	0
select * from performance_schema.global_variables where variable_name='rand_seed2';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='rand_seed2';
VARIABLE_NAME	VARIABLE_VALUE
rand_seed2	0
set session rand_seed2=1;
select @@session.rand_seed2;
@@session.rand_seed2
0
select * from performance_schema.global_variables where variable_name='rand_seed2';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='rand_seed2';
VARIABLE_NAME	VARIABLE_VALUE
rand_seed2	0
set global rand_seed2=1;
ERROR HY000: Variable 'rand_seed2' is a SESSION variable and can't be used with SET GLOBAL
set session rand_seed2=1.1;
ERROR 42000: Incorrect argument type to variable 'rand_seed2'
set session rand_seed2=1e1;
ERROR 42000: Incorrect argument type to variable 'rand_seed2'
set session rand_seed2="foo";
ERROR 42000: Incorrect argument type to variable 'rand_seed2'
