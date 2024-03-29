select @@global.debug_sensitive_session_string;
@@global.debug_sensitive_session_string

select @@global.debug_sensitive_session_string into @save;
show global variables like 'debug_sensitive_session_string';
Variable_name	Value
debug_sensitive_session_string	#
select * from performance_schema.global_variables where variable_name="debug_sensitive_session_string";
VARIABLE_NAME	VARIABLE_VALUE
debug_sensitive_session_string	#
set @@global.debug_sensitive_session_string="1";
set @@global.debug_sensitive_session_string=@save;
set @@global.debug_sensitive_session_string=1;
ERROR 42000: Incorrect argument type to variable 'debug_sensitive_session_string'
set @@global.debug_sensitive_session_string=1.1;
ERROR 42000: Incorrect argument type to variable 'debug_sensitive_session_string'
set @@global.debug_sensitive_session_string=1e1;
ERROR 42000: Incorrect argument type to variable 'debug_sensitive_session_string'
