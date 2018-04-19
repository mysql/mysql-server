select @@global.large_page_size = 1;
@@global.large_page_size = 1
0
select @@session.large_page_size;
ERROR HY000: Variable 'large_page_size' is a GLOBAL variable
show global variables like 'large_page_size';
Variable_name	Value
large_page_size	#
show session variables like 'large_page_size';
Variable_name	Value
large_page_size	#
select * from performance_schema.global_variables where variable_name='large_page_size';
VARIABLE_NAME	VARIABLE_VALUE
large_page_size	#
select * from performance_schema.session_variables where variable_name='large_page_size';
VARIABLE_NAME	VARIABLE_VALUE
large_page_size	#
set global large_page_size=1;
ERROR HY000: Variable 'large_page_size' is a read only variable
set session large_page_size=1;
ERROR HY000: Variable 'large_page_size' is a read only variable
