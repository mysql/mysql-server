SELECT @@global.optimizer_trace_offset;
@@global.optimizer_trace_offset
-1
SET @@global.optimizer_trace_offset=5;
show global variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	5
select * from performance_schema.global_variables where variable_name='optimizer_trace_offset';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_offset	5
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
-1
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-1
select * from performance_schema.session_variables where variable_name='optimizer_trace_offset';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_offset	-1
set session optimizer_trace_offset=10;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
10
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	10
set session optimizer_trace_offset=0;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
0
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	0
set session optimizer_trace_offset=-100;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
-100
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-100
set session optimizer_trace_offset=2000000000;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
2000000000
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	2000000000
set session optimizer_trace_offset=-2000000000;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
-2000000000
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-2000000000
set session optimizer_trace_offset=default;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
5
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	5
set global optimizer_trace_offset=default;
select @@global.optimizer_trace_offset;
@@global.optimizer_trace_offset
-1
show global variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-1
set optimizer_trace_offset=1.1;
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_offset'
set optimizer_trace_offset="foobar";
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_offset'
