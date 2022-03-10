alter table performance_schema.global_variables
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.global_variables;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'global_variables'
ALTER TABLE performance_schema.global_variables
ADD INDEX test_index(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.global_variables(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.global_variables;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.global_variables;
DROP TABLE test.create_select;
