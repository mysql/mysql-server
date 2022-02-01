set @save.slave_pending_jobs_size_max= @@global.slave_pending_jobs_size_max;
select @@session.slave_pending_jobs_size_max;
ERROR HY000: Variable 'slave_pending_jobs_size_max' is a GLOBAL variable
show global variables like 'slave_pending_jobs_size_max';
Variable_name	Value
slave_pending_jobs_size_max	134217728
show session variables like 'slave_pending_jobs_size_max';
Variable_name	Value
slave_pending_jobs_size_max	134217728
select * from performance_schema.global_variables where variable_name='$var';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='$var';
VARIABLE_NAME	VARIABLE_VALUE
set @@global.slave_pending_jobs_size_max= 0;
Warnings:
Warning	1292	Truncated incorrect slave_pending_jobs_size_max value: '0'
select @@global.slave_pending_jobs_size_max;
@@global.slave_pending_jobs_size_max
1024
set @@global.slave_pending_jobs_size_max= 1.1;
ERROR 42000: Incorrect argument type to variable 'slave_pending_jobs_size_max'
set @@global.slave_pending_jobs_size_max= "foo";
ERROR 42000: Incorrect argument type to variable 'slave_pending_jobs_size_max'
set @@global.slave_pending_jobs_size_max= 0;
Warnings:
Warning	1292	Truncated incorrect slave_pending_jobs_size_max value: '0'
set @@global.slave_pending_jobs_size_max= cast(-1 as unsigned int);
Warnings:
Warning	1292	Truncated incorrect slave_pending_jobs_size_max value: '18446744073709551615'
select @@global.slave_pending_jobs_size_max as "truncated to the maximum";
truncated to the maximum
18446744073709550592
set @@global.slave_pending_jobs_size_max= @save.slave_pending_jobs_size_max;
