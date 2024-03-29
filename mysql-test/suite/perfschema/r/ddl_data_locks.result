alter table performance_schema.data_locks add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.data_locks;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'data_locks'
ALTER TABLE performance_schema.data_locks ADD INDEX test_index(LOCK_DATA);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.data_locks(LOCK_DATA);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.data_locks;
DROP TABLE test.create_select;
