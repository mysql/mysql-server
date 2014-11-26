select * from performance_schema.user_variables_by_thread
where variable_name like 'foo%' limit 1;
select * from performance_schema.user_variables_by_thread
where variable_name='FOO';
insert into performance_schema.user_variables_by_thread
set thread_id=12, variable_name='foo', variable_value='bar';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
update performance_schema.user_variables_by_thread
set thread_id=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
update performance_schema.user_variables_by_thread
set thread_id=12 where variable_name like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
delete from performance_schema.user_variables_by_thread
where thread_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
delete from performance_schema.user_variables_by_thread;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
LOCK TABLES performance_schema.user_variables_by_thread READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
UNLOCK TABLES;
LOCK TABLES performance_schema.user_variables_by_thread WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'user_variables_by_thread'
UNLOCK TABLES;
CREATE TRIGGER test_uvar_trigger
AFTER INSERT ON performance_schema.user_variables_by_thread
FOR EACH ROW BEGIN
set @a='OK';
END|
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
