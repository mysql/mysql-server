select * from performance_schema.rwlock_instances limit 1;
select * from performance_schema.rwlock_instances
where name='FOO';
insert into performance_schema.rwlock_instances
set name='FOO', object_instance_begin=12;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'rwlock_instances'
update performance_schema.rwlock_instances
set name='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'rwlock_instances'
delete from performance_schema.rwlock_instances
where name like "wait/%";
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'rwlock_instances'
delete from performance_schema.rwlock_instances;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'rwlock_instances'
LOCK TABLES performance_schema.rwlock_instances READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'rwlock_instances'
UNLOCK TABLES;
LOCK TABLES performance_schema.rwlock_instances WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'rwlock_instances'
UNLOCK TABLES;
