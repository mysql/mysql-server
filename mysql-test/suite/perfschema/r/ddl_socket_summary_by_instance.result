alter table performance_schema.socket_summary_by_instance add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.socket_summary_by_instance;
ALTER TABLE performance_schema.socket_summary_by_instance ADD INDEX test_index(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.socket_summary_by_instance(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.socket_summary_by_instance;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.socket_summary_by_instance;
DROP TABLE test.create_select;
