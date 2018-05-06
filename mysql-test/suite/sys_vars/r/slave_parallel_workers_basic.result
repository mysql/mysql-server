set @save.slave_parallel_workers= @@global.slave_parallel_workers;
select @@session.slave_parallel_workers;
ERROR HY000: Variable 'slave_parallel_workers' is a GLOBAL variable
select variable_name from performance_schema.global_variables where variable_name='$var';
variable_name
select variable_name from performance_schema.session_variables where variable_name='$var';
variable_name
set @@global.slave_parallel_workers= 0;
select @@global.slave_parallel_workers;
@@global.slave_parallel_workers
0
set @@global.slave_parallel_workers= 1.1;
ERROR 42000: Incorrect argument type to variable 'slave_parallel_workers'
set @@global.slave_parallel_workers= "foo";
ERROR 42000: Incorrect argument type to variable 'slave_parallel_workers'
set @@global.slave_parallel_workers= 0;
set @@global.slave_parallel_workers= cast(-1 as unsigned int);
Warnings:
Warning	1292	Truncated incorrect slave_parallel_workers value: '18446744073709551615'
select @@global.slave_parallel_workers as "truncated to the maximum";
truncated to the maximum
1024
set @@global.slave_parallel_workers= @save.slave_parallel_workers;
