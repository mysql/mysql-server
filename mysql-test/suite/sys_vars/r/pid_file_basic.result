select @@global.pid_file;
@@global.pid_file
MYSQLTEST_VARDIR/run/mysqld.1.pid
select @@session.pid_file;
ERROR HY000: Variable 'pid_file' is a GLOBAL variable
show global variables like 'pid_file';
Variable_name	Value
pid_file	MYSQLTEST_VARDIR/run/mysqld.1.pid
show session variables like 'pid_file';
Variable_name	Value
pid_file	MYSQLTEST_VARDIR/run/mysqld.1.pid
select * from performance_schema.global_variables where variable_name='pid_file';
VARIABLE_NAME	VARIABLE_VALUE
pid_file	MYSQLTEST_VARDIR/run/mysqld.1.pid
select * from performance_schema.session_variables where variable_name='pid_file';
VARIABLE_NAME	VARIABLE_VALUE
pid_file	MYSQLTEST_VARDIR/run/mysqld.1.pid
set global pid_file=1;
ERROR HY000: Variable 'pid_file' is a read only variable
set session pid_file=1;
ERROR HY000: Variable 'pid_file' is a read only variable
