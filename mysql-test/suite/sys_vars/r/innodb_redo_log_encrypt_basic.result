SET @start_global_value = @@global.innodb_redo_log_encrypt;
SELECT @start_global_value;
@start_global_value
0
select @@global.innodb_redo_log_encrypt in (0, 1);
@@global.innodb_redo_log_encrypt in (0, 1)
1
select @@global.innodb_redo_log_encrypt;
@@global.innodb_redo_log_encrypt
0
select @@session.innodb_redo_log_encrypt;
ERROR HY000: Variable 'innodb_redo_log_encrypt' is a GLOBAL variable
show global variables like 'innodb_redo_log_encrypt';
Variable_name	Value
innodb_redo_log_encrypt	OFF
show session variables like 'innodb_redo_log_encrypt';
Variable_name	Value
innodb_redo_log_encrypt	OFF
select * from performance_schema.global_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
select * from performance_schema.session_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
set global innodb_redo_log_encrypt=1;
select * from performance_schema.global_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
select * from performance_schema.session_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
set @@global.innodb_redo_log_encrypt=0;
select @@global.innodb_redo_log_encrypt;
@@global.innodb_redo_log_encrypt
0
select * from performance_schema.global_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
select * from performance_schema.session_variables where variable_name='innodb_redo_log_encrypt';
VARIABLE_NAME	VARIABLE_VALUE
innodb_redo_log_encrypt	OFF
set session innodb_redo_log_encrypt='some';
ERROR HY000: Variable 'innodb_redo_log_encrypt' is a GLOBAL variable and should be set with SET GLOBAL
set @@session.innodb_redo_log_encrypt='some';
ERROR HY000: Variable 'innodb_redo_log_encrypt' is a GLOBAL variable and should be set with SET GLOBAL
set global innodb_redo_log_encrypt=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_redo_log_encrypt'
set global innodb_redo_log_encrypt='foo';
ERROR 42000: Variable 'innodb_redo_log_encrypt' can't be set to the value of 'foo'
set global innodb_redo_log_encrypt=-2;
ERROR 42000: Variable 'innodb_redo_log_encrypt' can't be set to the value of '-2'
set global innodb_redo_log_encrypt=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_redo_log_encrypt'
set global innodb_redo_log_encrypt=2;
ERROR 42000: Variable 'innodb_redo_log_encrypt' can't be set to the value of '2'
SET @@global.innodb_redo_log_encrypt = @start_global_value;
SELECT @@global.innodb_redo_log_encrypt;
@@global.innodb_redo_log_encrypt
0
