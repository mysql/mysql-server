alter table performance_schema.variables_by_thread
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.variables_by_thread;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'variables_by_thread'
ALTER TABLE performance_schema.variables_by_thread
ADD INDEX test_index(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.variables_by_thread(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.variables_by_thread;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
