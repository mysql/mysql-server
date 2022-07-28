select @@global.persist_sensitive_variables_in_plaintext;
@@global.persist_sensitive_variables_in_plaintext
1
show global variables like 'persist_sensitive_variables_in_plaintext';
Variable_name	Value
persist_sensitive_variables_in_plaintext	#
select * from performance_schema.global_variables where variable_name="persist_sensitive_variables_in_plaintext";
VARIABLE_NAME	VARIABLE_VALUE
persist_sensitive_variables_in_plaintext	#
set @@global.persist_sensitive_variables_in_plaintext="1";
ERROR HY000: Variable 'persist_sensitive_variables_in_plaintext' is a read only variable
set @@global.persist_sensitive_variables_in_plaintext=1;
ERROR HY000: Variable 'persist_sensitive_variables_in_plaintext' is a read only variable
set @@global.persist_sensitive_variables_in_plaintext=1.1;
ERROR HY000: Variable 'persist_sensitive_variables_in_plaintext' is a read only variable
set @@global.persist_sensitive_variables_in_plaintext=1e1;
ERROR HY000: Variable 'persist_sensitive_variables_in_plaintext' is a read only variable
