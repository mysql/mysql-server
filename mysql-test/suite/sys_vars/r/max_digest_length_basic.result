select @@global.max_digest_length;
@@global.max_digest_length
123
select @@session.max_digest_length;
ERROR HY000: Variable 'max_digest_length' is a GLOBAL variable
show global variables like 'max_digest_length';
Variable_name	Value
max_digest_length	123
show session variables like 'max_digest_length';
Variable_name	Value
max_digest_length	123
select * from performance_schema.global_variables
where variable_name='max_digest_length';
VARIABLE_NAME	VARIABLE_VALUE
max_digest_length	123
select * from performance_schema.session_variables
where variable_name='max_digest_length';
VARIABLE_NAME	VARIABLE_VALUE
max_digest_length	123
set global max_digest_length=1;
ERROR HY000: Variable 'max_digest_length' is a read only variable
set session max_digest_length=1;
ERROR HY000: Variable 'max_digest_length' is a read only variable
