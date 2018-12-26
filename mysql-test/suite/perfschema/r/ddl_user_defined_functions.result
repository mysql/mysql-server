alter table performance_schema.user_defined_functions
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.user_defined_functions;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'user_defined_functions'
ALTER TABLE performance_schema.user_defined_functions
ADD INDEX test_index(IP);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.user_defined_functions(UDF_TYPE);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.user_defined_functions;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
