alter table performance_schema.file_instances add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.file_instances;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'file_instances'
ALTER TABLE performance_schema.file_instances ADD INDEX test_index(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.file_instances(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.file_instances;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
