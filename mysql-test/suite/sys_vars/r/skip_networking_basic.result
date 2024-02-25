select @@global.skip_networking;
@@global.skip_networking
0
select @@session.skip_networking;
ERROR HY000: Variable 'skip_networking' is a GLOBAL variable
show global variables like 'skip_networking';
Variable_name	Value
skip_networking	OFF
show session variables like 'skip_networking';
Variable_name	Value
skip_networking	OFF
select * from performance_schema.global_variables where variable_name='skip_networking';
VARIABLE_NAME	VARIABLE_VALUE
skip_networking	OFF
select * from performance_schema.session_variables where variable_name='skip_networking';
VARIABLE_NAME	VARIABLE_VALUE
skip_networking	OFF
set global skip_networking=1;
ERROR HY000: Variable 'skip_networking' is a read only variable
set session skip_networking=1;
ERROR HY000: Variable 'skip_networking' is a read only variable
