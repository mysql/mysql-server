select @@global.admin_address;
@@global.admin_address
NULL
select @@session.admin_address;
ERROR HY000: Variable 'admin_address' is a GLOBAL variable
show global variables like 'admin_address';
Variable_name	Value
admin_address	
show session variables like 'admin_address';
Variable_name	Value
admin_address	
select * from performance_schema.global_variables where variable_name='admin_address';
VARIABLE_NAME	VARIABLE_VALUE
admin_address	
select * from performance_schema.session_variables where variable_name='admin_address';
VARIABLE_NAME	VARIABLE_VALUE
admin_address	
select * from performance_schema.variables_info where variable_name='admin_address';
VARIABLE_NAME	VARIABLE_SOURCE	VARIABLE_PATH	MIN_VALUE	MAX_VALUE	SET_TIME	SET_USER	SET_HOST
admin_address	COMPILED		0	0	NULL	NULL	NULL
set global admin_address='::1';
ERROR HY000: Variable 'admin_address' is a read only variable
set session admin_port='::1';
ERROR HY000: Variable 'admin_port' is a read only variable
set persist admin_port='::1';
ERROR HY000: Variable 'admin_port' is a read only variable
# restart: --skip-name-resolve --admin-address=127.0.0.1 --admin_port=ADMIN_PORT
select * from performance_schema.global_variables where variable_name='admin_address';
VARIABLE_NAME	VARIABLE_VALUE
admin_address	127.0.0.1
select * from performance_schema.session_variables where variable_name='admin_address';
VARIABLE_NAME	VARIABLE_VALUE
admin_address	127.0.0.1
#
# Starting mysqld in the regular mode...
#
# restart
