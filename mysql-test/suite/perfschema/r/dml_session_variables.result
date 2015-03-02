select * from performance_schema.session_variables
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.session_variables
where variable_name='FOO';
insert into performance_schema.session_variables
set variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'session_variables'
update performance_schema.session_variables
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'session_variables'
delete from performance_schema.session_variables
where variable_name <> 'FOO';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'session_variables'
delete from performance_schema.session_variables;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'session_variables'
LOCK TABLES performance_schema.session_variables READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'session_variables'
UNLOCK TABLES;
LOCK TABLES performance_schema.session_variables WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'session_variables'
UNLOCK TABLES;
