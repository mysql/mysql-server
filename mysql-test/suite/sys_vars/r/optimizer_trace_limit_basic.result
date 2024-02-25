SELECT @@global.optimizer_trace_limit;
@@global.optimizer_trace_limit
1
SET @@global.optimizer_trace_limit=5;
show global variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	5
select * from performance_schema.global_variables where variable_name='optimizer_trace_limit';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_limit	5
select @@session.optimizer_trace_limit;
@@session.optimizer_trace_limit
1
show session variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	1
select * from performance_schema.session_variables where variable_name='optimizer_trace_limit';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_limit	1
set session optimizer_trace_limit=10;
select @@session.optimizer_trace_limit;
@@session.optimizer_trace_limit
10
show session variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	10
set session optimizer_trace_limit=0;
select @@session.optimizer_trace_limit;
@@session.optimizer_trace_limit
0
show session variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	0
set session optimizer_trace_limit=-100;
Warnings:
Warning	1292	Truncated incorrect optimizer_trace_limit value: '-100'
select @@session.optimizer_trace_limit;
@@session.optimizer_trace_limit
0
show session variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	0
set session optimizer_trace_limit=default;
select @@session.optimizer_trace_limit;
@@session.optimizer_trace_limit
5
show session variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	5
set global optimizer_trace_limit=default;
select @@global.optimizer_trace_limit;
@@global.optimizer_trace_limit
1
show global variables like 'optimizer_trace_limit';
Variable_name	Value
optimizer_trace_limit	1
set optimizer_trace_limit=1.1;
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_limit'
set optimizer_trace_limit="foobar";
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_limit'
