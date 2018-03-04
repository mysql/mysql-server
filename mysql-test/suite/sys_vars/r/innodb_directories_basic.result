Valid values are semi-colon separated strings
select @@global.innodb_directories;
@@global.innodb_directories
NULL
select @@session.innodb_directories;
ERROR HY000: Variable 'innodb_directories' is a GLOBAL variable
show global variables like 'innodb_directories';
Variable_name	Value
innodb_directories	
show session variables like 'innodb_directories';
Variable_name	Value
innodb_directories	
select * from performance_schema.global_variables where variable_name='innodb_directories';
VARIABLE_NAME	VARIABLE_VALUE
innodb_directories	
select * from performance_schema.session_variables where variable_name='innodb_directories';
VARIABLE_NAME	VARIABLE_VALUE
innodb_directories	
set global innodb_directories='a;b;c';
ERROR HY000: Variable 'innodb_directories' is a read only variable
set session innodb_directories=1;
ERROR HY000: Variable 'innodb_directories' is a read only variable
