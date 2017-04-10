set session debug="L";
select @@global.debug="1";
@@global.debug="1"
0
select @@session.debug;
@@session.debug
L
show global variables like 'debug';
Variable_name	Value
debug	#
show session variables like 'debug';
Variable_name	Value
debug	L
select * from performance_schema.global_variables where variable_name="debug";
VARIABLE_NAME	VARIABLE_VALUE
debug	#
select * from performance_schema.session_variables where variable_name="debug";
VARIABLE_NAME	VARIABLE_VALUE
debug	L
set @@global.debug=1;
ERROR 42000: Incorrect argument type to variable 'debug'
set @@global.debug=1.1;
ERROR 42000: Incorrect argument type to variable 'debug'
set @@global.debug=1e1;
ERROR 42000: Incorrect argument type to variable 'debug'
