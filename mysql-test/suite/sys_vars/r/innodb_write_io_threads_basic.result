select @@global.innodb_write_io_threads;
@@global.innodb_write_io_threads
4
select @@session.innodb_write_io_threads;
ERROR HY000: Variable 'innodb_write_io_threads' is a GLOBAL variable
show global variables like 'innodb_write_io_threads';
Variable_name	Value
innodb_write_io_threads	4
show session variables like 'innodb_write_io_threads';
Variable_name	Value
innodb_write_io_threads	4
select * from performance_schema.global_variables where variable_name='innodb_write_io_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_write_io_threads	4
select * from performance_schema.session_variables where variable_name='innodb_write_io_threads';
VARIABLE_NAME	VARIABLE_VALUE
innodb_write_io_threads	4
set global innodb_write_io_threads=1;
ERROR HY000: Variable 'innodb_write_io_threads' is a read only variable
set session innodb_write_io_threads=1;
ERROR HY000: Variable 'innodb_write_io_threads' is a read only variable
