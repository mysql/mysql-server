select @@global.old;
@@global.old
0
select @@session.old;
ERROR HY000: Variable 'old' is a GLOBAL variable
show global variables like 'old';
Variable_name	Value
old	OFF
show session variables like 'old';
Variable_name	Value
old	OFF
select * from performance_schema.global_variables where variable_name='old';
VARIABLE_NAME	VARIABLE_VALUE
old	OFF
select * from performance_schema.session_variables where variable_name='old';
VARIABLE_NAME	VARIABLE_VALUE
old	OFF
set global old=1;
ERROR HY000: Variable 'old' is a read only variable
set session old=1;
ERROR HY000: Variable 'old' is a read only variable
