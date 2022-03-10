alter table performance_schema.rwlock_instances add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.rwlock_instances;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'rwlock_instances'
ALTER TABLE performance_schema.rwlock_instances ADD INDEX test_index(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.rwlock_instances(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.rwlock_instances;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.rwlock_instances;
DROP TABLE test.create_select;
