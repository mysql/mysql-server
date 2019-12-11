select @@global.innodb_doublewrite_files in (2, 64);
@@global.innodb_doublewrite_files in (2, 64)
1
select @@global.innodb_doublewrite_files;
@@global.innodb_doublewrite_files
2
show global variables like 'innodb_doublewrite_files';
Variable_name	Value
innodb_doublewrite_files	2
select * from performance_schema.global_variables where variable_name='innodb_doublewrite_files';
VARIABLE_NAME	VARIABLE_VALUE
innodb_doublewrite_files	2
set global innodb_doublewrite_files=3;
ERROR HY000: Variable 'innodb_doublewrite_files' is a read only variable
select @@global.innodb_doublewrite_files;
@@global.innodb_doublewrite_files
2
select * from performance_schema.global_variables where variable_name='innodb_doublewrite_files';
VARIABLE_NAME	VARIABLE_VALUE
innodb_doublewrite_files	2
