select @@global.report_password;
@@global.report_password
NULL
select @@session.report_password;
ERROR HY000: Variable 'report_password' is a GLOBAL variable
show global variables like 'report_password';
Variable_name	Value
report_password	
show session variables like 'report_password';
Variable_name	Value
report_password	
select * from performance_schema.global_variables where variable_name='report_password';
VARIABLE_NAME	VARIABLE_VALUE
report_password	
select * from performance_schema.session_variables where variable_name='report_password';
VARIABLE_NAME	VARIABLE_VALUE
report_password	
set global report_password=1;
ERROR HY000: Variable 'report_password' is a read only variable
set session report_password=1;
ERROR HY000: Variable 'report_password' is a read only variable
