select @@global.last_insert_id;
ERROR HY000: Variable 'last_insert_id' is a SESSION variable
select @@session.last_insert_id;
@@session.last_insert_id
0
show global variables like 'last_insert_id';
Variable_name	Value
show session variables like 'last_insert_id';
Variable_name	Value
last_insert_id	0
select * from performance_schema.global_variables where variable_name='last_insert_id';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='last_insert_id';
VARIABLE_NAME	VARIABLE_VALUE
last_insert_id	0
set global last_insert_id=99;
ERROR HY000: Variable 'last_insert_id' is a SESSION variable and can't be used with SET GLOBAL
set session last_insert_id=42;
select @@global.last_insert_id;
ERROR HY000: Variable 'last_insert_id' is a SESSION variable
select @@session.last_insert_id;
@@session.last_insert_id
42
show global variables like 'last_insert_id';
Variable_name	Value
show session variables like 'last_insert_id';
Variable_name	Value
last_insert_id	42
select * from performance_schema.global_variables where variable_name='last_insert_id';
VARIABLE_NAME	VARIABLE_VALUE
select * from performance_schema.session_variables where variable_name='last_insert_id';
VARIABLE_NAME	VARIABLE_VALUE
last_insert_id	42
set session last_insert_id=1.1;
ERROR 42000: Incorrect argument type to variable 'last_insert_id'
set session last_insert_id=1e1;
ERROR 42000: Incorrect argument type to variable 'last_insert_id'
set session last_insert_id="foo";
ERROR 42000: Incorrect argument type to variable 'last_insert_id'
