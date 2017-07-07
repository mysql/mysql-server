alter table performance_schema.session_status
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.session_status;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'session_status'
ALTER TABLE performance_schema.session_status
ADD INDEX test_index(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.session_status(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.session_status;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
