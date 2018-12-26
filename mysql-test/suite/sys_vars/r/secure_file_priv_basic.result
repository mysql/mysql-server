select @@global.secure_file_priv;
@@global.secure_file_priv
MYSQLTEST_VARDIR/
select @@session.secure_file_priv;
ERROR HY000: Variable 'secure_file_priv' is a GLOBAL variable
show global variables like 'secure_file_priv';
Variable_name	Value
secure_file_priv	MYSQLTEST_VARDIR/
show session variables like 'secure_file_priv';
Variable_name	Value
secure_file_priv	MYSQLTEST_VARDIR/
select * from performance_schema.global_variables where variable_name='secure_file_priv';
VARIABLE_NAME	VARIABLE_VALUE
secure_file_priv	MYSQLTEST_VARDIR/
select * from performance_schema.session_variables where variable_name='secure_file_priv';
VARIABLE_NAME	VARIABLE_VALUE
secure_file_priv	MYSQLTEST_VARDIR/
set global secure_file_priv=1;
ERROR HY000: Variable 'secure_file_priv' is a read only variable
set session secure_file_priv=1;
ERROR HY000: Variable 'secure_file_priv' is a read only variable
