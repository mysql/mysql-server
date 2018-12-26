select * from performance_schema.status_by_host
where variable_name like 'bytes_%' limit 1;
select * from performance_schema.status_by_host
where variable_name='FOO';
insert into performance_schema.status_by_host
set host='HOST', variable_name='FOO', variable_value='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'status_by_host'
update performance_schema.status_by_host
set variable_name='FOO', variable_value='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_host'
update performance_schema.status_by_host
set variable_name='FOO' where host <> 'HOST';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'status_by_host'
delete from performance_schema.status_by_host
where host <> 'HOST';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_host'
delete from performance_schema.status_by_host;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'status_by_host'
LOCK TABLES performance_schema.status_by_host READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_host'
UNLOCK TABLES;
LOCK TABLES performance_schema.status_by_host WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'status_by_host'
UNLOCK TABLES;
