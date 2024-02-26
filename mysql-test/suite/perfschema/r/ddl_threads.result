alter table performance_schema.threads add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.threads;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'threads'
ALTER TABLE performance_schema.threads ADD INDEX test_index(PROCESSLIST_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.threads(PROCESSLIST_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.threads;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.threads;
DROP TABLE test.create_select;
