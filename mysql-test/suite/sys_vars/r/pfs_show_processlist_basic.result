select @@global.performance_schema_show_processlist into @save;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
0
select @@session.performance_schema_show_processlist;
ERROR HY000: Variable 'performance_schema_show_processlist' is a GLOBAL variable
show global variables like 'performance_schema_show_processlist';
Variable_name	Value
performance_schema_show_processlist	OFF
show session variables like 'performance_schema_show_processlist';
Variable_name	Value
performance_schema_show_processlist	OFF
select * from performance_schema.global_variables
where variable_name='performance_schema_show_processlist';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_show_processlist	OFF
select * from performance_schema.session_variables
where variable_name='performance_schema_show_processlist';
VARIABLE_NAME	VARIABLE_VALUE
performance_schema_show_processlist	OFF
set global performance_schema_show_processlist = 99;
ERROR 42000: Variable 'performance_schema_show_processlist' can't be set to the value of '99'
set global performance_schema_show_processlist = 'FOO';
ERROR 42000: Variable 'performance_schema_show_processlist' can't be set to the value of 'FOO'
set global performance_schema_show_processlist = 1;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
1
set global performance_schema_show_processlist = 0;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
0
set global performance_schema_show_processlist = ON;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
1
set global performance_schema_show_processlist = OFF;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
0
set global performance_schema_show_processlist = TRUE;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
1
set global performance_schema_show_processlist = FALSE;
select @@global.performance_schema_show_processlist;
@@global.performance_schema_show_processlist
0
set @@global.performance_schema_show_processlist = @save;
