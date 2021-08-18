alter table performance_schema.table_handles add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.table_handles;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'table_handles'
ALTER TABLE performance_schema.table_handles ADD INDEX test_index(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.table_handles(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.table_handles;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.table_handles;
DROP TABLE test.create_select;
