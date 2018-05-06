select @@global.schema_definition_cache;
@@global.schema_definition_cache
256
select @@session.schema_definition_cache;
ERROR HY000: Variable 'schema_definition_cache' is a GLOBAL variable
show global variables like 'schema_definition_cache';
Variable_name	Value
schema_definition_cache	256
show session variables like 'schema_definition_cache';
Variable_name	Value
schema_definition_cache	256
select * from performance_schema.global_variables where variable_name='schema_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
schema_definition_cache	256
select * from performance_schema.session_variables where variable_name='schema_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
schema_definition_cache	256
