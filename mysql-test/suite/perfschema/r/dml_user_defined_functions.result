select * from performance_schema.user_defined_functions
where UDF_NAME like '%' limit 1;
select * from performance_schema.user_defined_functions
where UDF_NAME='niamatakava';
insert into performance_schema.user_defined_functions
set UDF_NAME='FOO', UDF_RETURN_TYPE='int', UDF_TYPE='function',
UDF_LIBRARY='bar', UDF_USAGE_COUNT=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'user_defined_functions'
update performance_schema.user_defined_functions
set UDF_USAGE_COUNT=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'user_defined_functions'
update performance_schema.user_defined_functions
set UDF_USAGE_COUNT=12 where UDF_NAME='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'user_defined_functions'
select UDF_USAGE_COUNT from performance_schema.user_defined_functions
where UDF_NAME='::1';
UDF_USAGE_COUNT
delete from performance_schema.user_defined_functions
where UDF_NAME='foo';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'user_defined_functions'
delete from performance_schema.user_defined_functions;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'user_defined_functions'
LOCK TABLES performance_schema.user_defined_functions READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'user_defined_functions'
UNLOCK TABLES;
LOCK TABLES performance_schema.user_defined_functions WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'user_defined_functions'
UNLOCK TABLES;
