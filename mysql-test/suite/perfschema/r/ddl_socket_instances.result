alter table performance_schema.socket_instances add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.socket_instances;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'socket_instances'
ALTER TABLE performance_schema.socket_instances ADD INDEX test_index(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.socket_instances(FILE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.socket_instances;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
