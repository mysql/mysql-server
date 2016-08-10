select * from performance_schema.variables_info
where variable_name like 'autocommit%' limit 1;
select * from performance_schema.variables_info
where variable_name='FOO';
insert into performance_schema.variables_info
set variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'variables_info'
update performance_schema.variables_info
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'variables_info'
delete from performance_schema.variables_info
where variable_name <> 'FOO';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'variables_info'
delete from performance_schema.variables_info;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'variables_info'
LOCK TABLES performance_schema.variables_info READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'variables_info'
UNLOCK TABLES;
LOCK TABLES performance_schema.variables_info WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'variables_info'
UNLOCK TABLES;
