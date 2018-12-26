select @@global.locked_in_memory;
@@global.locked_in_memory
0
select @@session.locked_in_memory;
ERROR HY000: Variable 'locked_in_memory' is a GLOBAL variable
show global variables like 'locked_in_memory';
Variable_name	Value
locked_in_memory	OFF
show session variables like 'locked_in_memory';
Variable_name	Value
locked_in_memory	OFF
select * from performance_schema.global_variables where variable_name='locked_in_memory';
VARIABLE_NAME	VARIABLE_VALUE
locked_in_memory	OFF
select * from performance_schema.session_variables where variable_name='locked_in_memory';
VARIABLE_NAME	VARIABLE_VALUE
locked_in_memory	OFF
set global locked_in_memory=1;
ERROR HY000: Variable 'locked_in_memory' is a read only variable
set session locked_in_memory=1;
ERROR HY000: Variable 'locked_in_memory' is a read only variable
