select * from performance_schema.hosts
limit 1;
select * from performance_schema.hosts
where host='FOO';
insert into performance_schema.hosts
set host='FOO',
current_connections=1, total_connections=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'hosts'
update performance_schema.hosts
set current_connections=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'hosts'
update performance_schema.hosts
set current_connections=12 where host like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'hosts'
delete from performance_schema.hosts
where total_connections=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'hosts'
delete from performance_schema.hosts;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'hosts'
LOCK TABLES performance_schema.hosts READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'hosts'
UNLOCK TABLES;
LOCK TABLES performance_schema.hosts WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'hosts'
UNLOCK TABLES;
