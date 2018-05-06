select @@global.stored_program_definition_cache;
@@global.stored_program_definition_cache
256
select @@session.stored_program_definition_cache;
ERROR HY000: Variable 'stored_program_definition_cache' is a GLOBAL variable
show global variables like 'stored_program_definition_cache';
Variable_name	Value
stored_program_definition_cache	256
show session variables like 'stored_program_definition_cache';
Variable_name	Value
stored_program_definition_cache	256
select * from performance_schema.global_variables where variable_name='stored_program_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
stored_program_definition_cache	256
select * from performance_schema.session_variables where variable_name='stored_program_definition_cache';
VARIABLE_NAME	VARIABLE_VALUE
stored_program_definition_cache	256
