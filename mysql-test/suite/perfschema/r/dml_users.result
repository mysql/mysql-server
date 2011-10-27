select * from performance_schema.users
limit 1;
select * from performance_schema.users
where user='FOO';
insert into performance_schema.users
set user='FOO',
current_connections=1, total_connections=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'users'
update performance_schema.users
set current_connections=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'users'
update performance_schema.users
set current_connections=12 where user like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'users'
delete from performance_schema.users
where total_connections=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'users'
delete from performance_schema.users;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'users'
LOCK TABLES performance_schema.users READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'users'
UNLOCK TABLES;
LOCK TABLES performance_schema.users WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'users'
UNLOCK TABLES;
