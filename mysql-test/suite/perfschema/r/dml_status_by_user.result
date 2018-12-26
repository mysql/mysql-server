select * from performance_schema.status_by_user
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.status_by_user
where variable_name='FOO';
insert into performance_schema.status_by_user
set user='USER', variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'status_by_user'
update performance_schema.status_by_user
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_user'
update performance_schema.status_by_user
set variable_name='FOO' where user <> 'USER';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_user'
delete from performance_schema.status_by_user
where user <> 'USER';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_user'
delete from performance_schema.status_by_user;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_user'
LOCK TABLES performance_schema.status_by_user READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_user'
UNLOCK TABLES;
LOCK TABLES performance_schema.status_by_user WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_user'
UNLOCK TABLES;
