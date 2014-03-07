select * from performance_schema.table_handles;
select * from performance_schema.table_handles
where object_name='foo';
OBJECT_TYPE	OBJECT_SCHEMA	OBJECT_NAME	OBJECT_INSTANCE_BEGIN	OWNER_THREAD_ID	OWNER_EVENT_ID	INTERNAL_LOCK	EXTERNAL_LOCK
insert into performance_schema.table_handles
set object_name='FOO', owner_event_id=1;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'table_handles'
update performance_schema.table_handles
set owner_event_id=12 where object_name='foo';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'table_handles'
delete from performance_schema.table_handles;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'table_handles'
delete from performance_schema.table_handles
where timer_name='CYCLE';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'table_handles'
LOCK TABLES performance_schema.table_handles READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'table_handles'
UNLOCK TABLES;
LOCK TABLES performance_schema.table_handles WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'table_handles'
UNLOCK TABLES;
