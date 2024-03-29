set session optimizer_trace_offset=10;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
10
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	10
set session optimizer_trace_offset=-10;
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
-10
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-10
set session optimizer_trace_offset=100;
Warnings:
Warning	1292	Truncated incorrect optimizer_trace_offset value: '100'
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
50
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	50
set session optimizer_trace_offset=-100;
Warnings:
Warning	1292	Truncated incorrect optimizer_trace_offset value: '-100'
select @@session.optimizer_trace_offset;
@@session.optimizer_trace_offset
-50
show session variables like 'optimizer_trace_offset';
Variable_name	Value
optimizer_trace_offset	-50
