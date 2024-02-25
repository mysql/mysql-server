select @@global.tablespace_definition_cache;
@@global.tablespace_definition_cache
256
select @@session.tablespace_definition_cache;
ERROR HY000: Variable 'tablespace_definition_cache' is a GLOBAL variable
show global variables like 'tablespace_definition_cache';
Variable_name	Value
tablespace_definition_cache	256
show session variables like 'tablespace_definition_cache';
Variable_name	Value
tablespace_definition_cache	256
select * from performance_schema.global_variables where variable_name='tablespace_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
tablespace_definition_cache	256
select * from performance_schema.session_variables where variable_name='tablespace_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
tablespace_definition_cache	256
