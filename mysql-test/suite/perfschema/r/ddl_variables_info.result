alter table performance_schema.variables_info
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.variables_info;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'variables_info'
ALTER TABLE performance_schema.variables_info
ADD INDEX test_index(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.variables_info(VARIABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.variables_info;
DROP TABLE test.create_select;
