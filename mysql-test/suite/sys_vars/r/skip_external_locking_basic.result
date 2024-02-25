select @@global.skip_external_locking;
@@global.skip_external_locking
1
select @@session.skip_external_locking;
ERROR HY000: Variable 'skip_external_locking' is a GLOBAL variable
show global variables like 'skip_external_locking';
Variable_name	Value
skip_external_locking	ON
show session variables like 'skip_external_locking';
Variable_name	Value
skip_external_locking	ON
select * from performance_schema.global_variables where variable_name='skip_external_locking';
VARIABLE_NAME	VARIABLE_VALUE
skip_external_locking	ON
select * from performance_schema.session_variables where variable_name='skip_external_locking';
VARIABLE_NAME	VARIABLE_VALUE
skip_external_locking	ON
set global skip_external_locking=1;
ERROR HY000: Variable 'skip_external_locking' is a read only variable
set session skip_external_locking=1;
ERROR HY000: Variable 'skip_external_locking' is a read only variable
