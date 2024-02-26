SET @start_global_value = @@global.innodb_tmpdir;
SELECT @start_global_value;
@start_global_value
NULL
select @@session.innodb_tmpdir;
@@session.innodb_tmpdir
NULL
show global variables like 'innodb_tmpdir';
Variable_name	Value
innodb_tmpdir	
show session variables like 'innodb_tmpdir';
Variable_name	Value
innodb_tmpdir	
select * from performance_schema.global_variables where variable_name='innodb_tmpdir';
VARIABLE_NAME	VARIABLE_VALUE
innodb_tmpdir	
select * from performance_schema.session_variables where variable_name='innodb_tmpdir';
VARIABLE_NAME	VARIABLE_VALUE
innodb_tmpdir	
set global innodb_tmpdir=@@global.tmpdir;
set session innodb_tmpdir=@@global.tmpdir;
set global innodb_tmpdir=1.1;
ERROR 42000: Incorrect argument type to variable 'innodb_tmpdir'
set global innodb_tmpdir=1e1;
ERROR 42000: Incorrect argument type to variable 'innodb_tmpdir'
set global innodb_tmpdir=repeat('a',1000);
ERROR 42000: Variable 'innodb_tmpdir' can't be set to the value of 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
show warnings;
Level	Code	Message
Warning	1210	Path length should not exceed 512 bytes
Error	1231	Variable 'innodb_tmpdir' can't be set to the value of 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
SET @@global.innodb_tmpdir = @start_global_value;
SELECT @@global.innodb_tmpdir;
@@global.innodb_tmpdir
NULL
