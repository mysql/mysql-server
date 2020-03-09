select @@global.admin_port;
@@global.admin_port
ADMIN_PORT
select @@session.admin_port;
ERROR HY000: Variable 'admin_port' is a GLOBAL variable
show global variables like 'admin_port';
Variable_name	Value
admin_port	ADMIN_PORT
show session variables like 'admin_port';
Variable_name	Value
admin_port	ADMIN_PORT
select * from performance_schema.global_variables where variable_name='admin_port';
VARIABLE_NAME	VARIABLE_VALUE
admin_port	ADMIN_PORT
select * from performance_schema.session_variables where variable_name='admin_port';
VARIABLE_NAME	VARIABLE_VALUE
admin_port	ADMIN_PORT
select variable_name, min_value, max_value from performance_schema.variables_info where variable_name='admin_port';
variable_name	min_value	max_value
admin_port	0	65535
set global admin_port=1000;
ERROR HY000: Variable 'admin_port' is a read only variable
set session admin_port=1000;
ERROR HY000: Variable 'admin_port' is a read only variable
set persist admin_port=1000;
ERROR HY000: Variable 'admin_port' is a read only variable
# restart: --skip-name-resolve --admin-address=127.0.0.1 --admin_port=ADMIN_PORT
select * from performance_schema.global_variables where variable_name='admin_port';
VARIABLE_NAME	VARIABLE_VALUE
admin_port	ADMIN_PORT
select * from performance_schema.session_variables where variable_name='admin_port';
VARIABLE_NAME	VARIABLE_VALUE
admin_port	ADMIN_PORT
# restart: --skip-name-resolve --admin_port=66000
SELECT @@admin_port;
@@admin_port
65535
# restart: --skip-name-resolve --admin_port=-1
call mtr.add_suppression("Warning.* option 'admin_port': value -1 adjusted to 0");
SELECT @@admin_port;
@@admin_port
33062
#
# Starting mysqld in the regular mode...
#
# restart
