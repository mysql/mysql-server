select @@global.myisam_mmap_size;
@@global.myisam_mmap_size
18446744073709551615
select @@session.myisam_mmap_size;
ERROR HY000: Variable 'myisam_mmap_size' is a GLOBAL variable
show global variables like 'myisam_mmap_size';
Variable_name	Value
myisam_mmap_size	18446744073709551615
show session variables like 'myisam_mmap_size';
Variable_name	Value
myisam_mmap_size	18446744073709551615
select * from performance_schema.global_variables where variable_name='myisam_mmap_size';
VARIABLE_NAME	VARIABLE_VALUE
myisam_mmap_size	18446744073709551615
select * from performance_schema.session_variables where variable_name='myisam_mmap_size';
VARIABLE_NAME	VARIABLE_VALUE
myisam_mmap_size	18446744073709551615
set global myisam_mmap_size=1;
ERROR HY000: Variable 'myisam_mmap_size' is a read only variable
set session myisam_mmap_size=1;
ERROR HY000: Variable 'myisam_mmap_size' is a read only variable
