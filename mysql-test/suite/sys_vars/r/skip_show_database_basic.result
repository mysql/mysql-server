select @@global.skip_show_database;
@@global.skip_show_database
0
select @@session.skip_show_database;
ERROR HY000: Variable 'skip_show_database' is a GLOBAL variable
show global variables like 'skip_show_database';
Variable_name	Value
skip_show_database	OFF
show session variables like 'skip_show_database';
Variable_name	Value
skip_show_database	OFF
select * from performance_schema.global_variables where variable_name='skip_show_database';
VARIABLE_NAME	VARIABLE_VALUE
skip_show_database	OFF
select * from performance_schema.session_variables where variable_name='skip_show_database';
VARIABLE_NAME	VARIABLE_VALUE
skip_show_database	OFF
set global skip_show_database=1;
ERROR HY000: Variable 'skip_show_database' is a read only variable
set session skip_show_database=1;
ERROR HY000: Variable 'skip_show_database' is a read only variable
