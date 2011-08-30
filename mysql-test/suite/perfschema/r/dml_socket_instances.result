select * from performance_schema.socket_instances limit 1;
select * from performance_schema.socket_instances
where ip='FOO';
insert into performance_schema.socket_instances
set ip='FOO', event_name='BAR', port=12;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'socket_instances'
update performance_schema.socket_instances
set ip='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'socket_instances'
delete from performance_schema.socket_instances
where event_name like "wait/%";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'socket_instances'
delete from performance_schema.socket_instances;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'socket_instances'
LOCK TABLES performance_schema.socket_instances READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'socket_instances'
UNLOCK TABLES;
LOCK TABLES performance_schema.socket_instances WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'socket_instances'
UNLOCK TABLES;
