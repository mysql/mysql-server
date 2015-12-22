select @@global.innodb_sort_buffer_size;
@@global.innodb_sort_buffer_size
1048576
select @@session.innodb_sort_buffer_size;
ERROR HY000: Variable 'innodb_sort_buffer_size' is a GLOBAL variable
show global variables like 'innodb_sort_buffer_size';
Variable_name	Value
innodb_sort_buffer_size	1048576
show session variables like 'innodb_sort_buffer_size';
Variable_name	Value
innodb_sort_buffer_size	1048576
select * from performance_schema.global_variables where variable_name='innodb_sort_buffer_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_sort_buffer_size	1048576
select * from performance_schema.session_variables where variable_name='innodb_sort_buffer_size';
VARIABLE_NAME	VARIABLE_VALUE
innodb_sort_buffer_size	1048576
set global innodb_sort_buffer_size=1;
ERROR HY000: Variable 'innodb_sort_buffer_size' is a read only variable
set session innodb_sort_buffer_size=1;
ERROR HY000: Variable 'innodb_sort_buffer_size' is a read only variable
