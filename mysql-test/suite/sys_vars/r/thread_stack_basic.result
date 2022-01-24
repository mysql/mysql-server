select @@global.thread_stack;
@@global.thread_stack
1048576
select @@session.thread_stack;
ERROR HY000: Variable 'thread_stack' is a GLOBAL variable
show global variables like 'thread_stack';
Variable_name	Value
thread_stack	1048576
show session variables like 'thread_stack';
Variable_name	Value
thread_stack	1048576
select * from performance_schema.global_variables where variable_name='thread_stack';
VARIABLE_NAME	VARIABLE_VALUE
thread_stack	1048576
select * from performance_schema.session_variables where variable_name='thread_stack';
VARIABLE_NAME	VARIABLE_VALUE
thread_stack	1048576
set global thread_stack=1;
ERROR HY000: Variable 'thread_stack' is a read only variable
set session thread_stack=1;
ERROR HY000: Variable 'thread_stack' is a read only variable
