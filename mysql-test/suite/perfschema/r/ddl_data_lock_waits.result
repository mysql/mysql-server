alter table performance_schema.data_lock_waits add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.data_lock_waits;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'data_lock_waits'
ALTER TABLE performance_schema.data_lock_waits ADD INDEX test_index(BLOCKING_THREAD_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.data_lock_waits(BLOCKING_THREAD_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.data_lock_waits;
DROP TABLE test.create_select;
