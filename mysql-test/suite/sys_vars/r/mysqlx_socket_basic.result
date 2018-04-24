select @@global.mysqlx_socket;
@@global.mysqlx_socket
MYSQL_TMP_DIR/mysqlx.1.sock
select @@session.mysqlx_socket;
ERROR HY000: Variable 'mysqlx_socket' is a GLOBAL variable
show global variables like 'mysqlx_socket';
Variable_name	Value
mysqlx_socket	MYSQL_TMP_DIR/mysqlx.1.sock
show session variables like 'mysqlx_socket';
Variable_name	Value
mysqlx_socket	MYSQL_TMP_DIR/mysqlx.1.sock
select * from performance_schema.global_variables where variable_name='mysqlx_socket';
VARIABLE_NAME	VARIABLE_VALUE
mysqlx_socket	MYSQL_TMP_DIR/mysqlx.1.sock
select * from performance_schema.session_variables where variable_name='mysqlx_socket';
VARIABLE_NAME	VARIABLE_VALUE
mysqlx_socket	MYSQL_TMP_DIR/mysqlx.1.sock
