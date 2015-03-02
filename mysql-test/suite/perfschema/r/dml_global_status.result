select * from performance_schema.global_status
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.global_status
where variable_name='FOO';
insert into performance_schema.global_status
set variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'global_status'
update performance_schema.global_status
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'global_status'
delete from performance_schema.global_status
where variable_name <> 'FOO';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'global_status'
delete from performance_schema.global_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'global_status'
LOCK TABLES performance_schema.global_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'global_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.global_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'global_status'
UNLOCK TABLES;
