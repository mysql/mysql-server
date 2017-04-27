select * from performance_schema.persisted_variables
where variable_name like 'autocommit%' limit 1;
select * from performance_schema.persisted_variables
where variable_name='FOO';
insert into performance_schema.persisted_variables
set variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'persisted_variables'
update performance_schema.persisted_variables
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'persisted_variables'
delete from performance_schema.persisted_variables
where variable_name <> 'FOO';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'persisted_variables'
delete from performance_schema.persisted_variables;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'persisted_variables'
LOCK TABLES performance_schema.persisted_variables READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'persisted_variables'
UNLOCK TABLES;
LOCK TABLES performance_schema.persisted_variables WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'persisted_variables'
UNLOCK TABLES;
