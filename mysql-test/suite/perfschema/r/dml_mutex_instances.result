select * from performance_schema.mutex_instances limit 1;
select * from performance_schema.mutex_instances
where name='FOO';
insert into performance_schema.mutex_instances
set name='FOO', object_instance_begin=12;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'mutex_instances'
update performance_schema.mutex_instances
set name='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'mutex_instances'
delete from performance_schema.mutex_instances
where name like "wait/%";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'mutex_instances'
delete from performance_schema.mutex_instances;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'mutex_instances'
LOCK TABLES performance_schema.mutex_instances READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'mutex_instances'
UNLOCK TABLES;
LOCK TABLES performance_schema.mutex_instances WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'mutex_instances'
UNLOCK TABLES;
