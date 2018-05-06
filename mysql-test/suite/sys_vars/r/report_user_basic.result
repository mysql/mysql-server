select @@global.report_user;
@@global.report_user
NULL
select @@session.report_user;
ERROR HY000: Variable 'report_user' is a GLOBAL variable
show global variables like 'report_user';
Variable_name	Value
report_user	
show session variables like 'report_user';
Variable_name	Value
report_user	
select * from performance_schema.global_variables where variable_name='report_user';
VARIABLE_NAME	VARIABLE_VALUE
report_user	
select * from performance_schema.session_variables where variable_name='report_user';
VARIABLE_NAME	VARIABLE_VALUE
report_user	
set global report_user=1;
ERROR HY000: Variable 'report_user' is a read only variable
set session report_user=1;
ERROR HY000: Variable 'report_user' is a read only variable
