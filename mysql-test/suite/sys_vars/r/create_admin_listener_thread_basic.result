select @@global.create_admin_listener_thread;
@@global.create_admin_listener_thread
0
select @@session.create_admin_listener_thread;
ERROR HY000: Variable 'create_admin_listener_thread' is a GLOBAL variable
show global variables like 'create_admin_listener_thread';
Variable_name	Value
create_admin_listener_thread	OFF
show session variables like 'create_admin_listener_thread';
Variable_name	Value
create_admin_listener_thread	OFF
select * from performance_schema.global_variables where variable_name='create_admin_listener_thread';
VARIABLE_NAME	VARIABLE_VALUE
create_admin_listener_thread	OFF
select * from performance_schema.session_variables where variable_name='create_admin_listener_thread';
VARIABLE_NAME	VARIABLE_VALUE
create_admin_listener_thread	OFF
select * from performance_schema.variables_info where variable_name='create_admin_listener_thread';
VARIABLE_NAME	VARIABLE_SOURCE	VARIABLE_PATH	MIN_VALUE	MAX_VALUE	SET_TIME	SET_USER	SET_HOST
create_admin_listener_thread	COMPILED		0	0	NULL	NULL	NULL
select * from performance_schema.threads where name="thread/sql/admin_interface";
set global create_admin_listener_thread=1;
ERROR HY000: Variable 'create_admin_listener_thread' is a read only variable
set session create_admin_listener_thread=1;
ERROR HY000: Variable 'create_admin_listener_thread' is a read only variable
set persist create_admin_listener_thread=1;
ERROR HY000: Variable 'create_admin_listener_thread' is a read only variable
# restart: --skip-name-resolve --admin-address=127.0.0.1 --admin_port=ADMIN_PORT --create-admin-listener-thread=1
select * from performance_schema.global_variables where variable_name='create_admin_listener_thread';
VARIABLE_NAME	VARIABLE_VALUE
create_admin_listener_thread	ON
select * from performance_schema.session_variables where variable_name='create_admin_listener_thread';
VARIABLE_NAME	VARIABLE_VALUE
create_admin_listener_thread	ON
#
# Starting mysqld in the regular mode...
#
# restart
