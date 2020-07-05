Valid values are semi-colon separated strings
select @@global.innodb_doublewrite_dir;
@@global.innodb_doublewrite_dir
NULL
select @@session.innodb_doublewrite_dir;
ERROR HY000: Variable 'innodb_doublewrite_dir' is a GLOBAL variable
show global variables like 'innodb_doublewrite_dir';
Variable_name	Value
innodb_doublewrite_dir	
show session variables like 'innodb_doublewrite_dir';
Variable_name	Value
innodb_doublewrite_dir	
select * from performance_schema.global_variables where variable_name='innodb_doublewrite_dir';
VARIABLE_NAME	VARIABLE_VALUE
innodb_doublewrite_dir	
select * from performance_schema.session_variables where variable_name='innodb_doublewrite_dir';
VARIABLE_NAME	VARIABLE_VALUE
innodb_doublewrite_dir	
set global innodb_doublewrite_dir='a;b;c';
ERROR HY000: Variable 'innodb_doublewrite_dir' is a read only variable
set session innodb_doublewrite_dir=1;
ERROR HY000: Variable 'innodb_doublewrite_dir' is a read only variable
