alter table performance_schema.metadata_locks add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.metadata_locks;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'metadata_locks'
ALTER TABLE performance_schema.metadata_locks ADD INDEX test_index(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.metadata_locks(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.metadata_locks;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
