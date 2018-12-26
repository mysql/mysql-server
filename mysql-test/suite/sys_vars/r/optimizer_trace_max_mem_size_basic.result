SELECT @@global.optimizer_trace_max_mem_size;
@@global.optimizer_trace_max_mem_size
1048576
SET @@global.optimizer_trace_max_mem_size=5;
show global variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	5
select * from performance_schema.global_variables where variable_name='optimizer_trace_max_mem_size';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_max_mem_size	5
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
1048576
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	1048576
select * from performance_schema.session_variables where variable_name='optimizer_trace_max_mem_size';
VARIABLE_NAME	VARIABLE_VALUE
optimizer_trace_max_mem_size	1048576
set session optimizer_trace_max_mem_size=10;
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
10
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	10
set session optimizer_trace_max_mem_size=0;
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
0
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	0
set session optimizer_trace_max_mem_size=-100;
Warnings:
Warning	1292	Truncated incorrect optimizer_trace_max_mem_size value: '-100'
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
0
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	0
set session optimizer_trace_max_mem_size=2000000000;
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
2000000000
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	2000000000
set session optimizer_trace_max_mem_size=default;
select @@session.optimizer_trace_max_mem_size;
@@session.optimizer_trace_max_mem_size
5
show session variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	5
set global optimizer_trace_max_mem_size=default;
select @@global.optimizer_trace_max_mem_size;
@@global.optimizer_trace_max_mem_size
1048576
show global variables like 'optimizer_trace_max_mem_size';
Variable_name	Value
optimizer_trace_max_mem_size	1048576
set optimizer_trace_max_mem_size=1.1;
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_max_mem_size'
set optimizer_trace_max_mem_size="foobar";
ERROR 42000: Incorrect argument type to variable 'optimizer_trace_max_mem_size'
