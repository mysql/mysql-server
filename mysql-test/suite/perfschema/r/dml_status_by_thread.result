select * from performance_schema.status_by_thread
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.status_by_thread
where variable_name='FOO';
insert into performance_schema.status_by_thread
set thread_id=1, variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'status_by_thread'
update performance_schema.status_by_thread
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_thread'
update performance_schema.status_by_thread
set variable_name='FOO' where thread_id=0;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_thread'
delete from performance_schema.status_by_thread
where thread_id=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_thread'
delete from performance_schema.status_by_thread;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_thread'
LOCK TABLES performance_schema.status_by_thread READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_thread'
UNLOCK TABLES;
LOCK TABLES performance_schema.status_by_thread WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_thread'
UNLOCK TABLES;
