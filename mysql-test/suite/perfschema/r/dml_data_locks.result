select * from performance_schema.data_locks;
select * from performance_schema.data_locks
where object_name='foo';
ENGINE	ENGINE_LOCK_ID	ENGINE_TRANSACTION_ID	THREAD_ID	EVENT_ID	OBJECT_SCHEMA	OBJECT_NAME	PARTITION_NAME	SUBPARTITION_NAME	INDEX_NAME	OBJECT_INSTANCE_BEGIN	LOCK_TYPE	LOCK_MODE	LOCK_STATUS	LOCK_DATA
insert into performance_schema.data_locks
set object_name='FOO', thread_id=1;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'data_locks'
update performance_schema.data_locks
set thread_id=12 where object_name='foo';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'data_locks'
delete from performance_schema.data_locks;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'data_locks'
delete from performance_schema.data_locks
where lock_mode='CYCLE';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'data_locks'
LOCK TABLES performance_schema.data_locks READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'data_locks'
UNLOCK TABLES;
LOCK TABLES performance_schema.data_locks WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'data_locks'
UNLOCK TABLES;
