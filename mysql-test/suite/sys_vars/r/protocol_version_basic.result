select @@global.protocol_version;
@@global.protocol_version
10
select @@session.protocol_version;
ERROR HY000: Variable 'protocol_version' is a GLOBAL variable
show global variables like 'protocol_version';
Variable_name	Value
protocol_version	10
show session variables like 'protocol_version';
Variable_name	Value
protocol_version	10
select * from performance_schema.global_variables where variable_name='protocol_version';
VARIABLE_NAME	VARIABLE_VALUE
protocol_version	10
select * from performance_schema.session_variables where variable_name='protocol_version';
VARIABLE_NAME	VARIABLE_VALUE
protocol_version	10
set global protocol_version=1;
ERROR HY000: Variable 'protocol_version' is a read only variable
set session protocol_version=1;
ERROR HY000: Variable 'protocol_version' is a read only variable
