alter table performance_schema.events_errors_summary_global_by_error
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_errors_summary_global_by_error;
ALTER TABLE performance_schema.events_errors_summary_global_by_error
ADD INDEX test_index(ERROR_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_errors_summary_global_by_error(ERROR_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `ERROR_NUMBER`
  ON performance_schema.events_errors_summary_global_by_error;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.events_errors_summary_global_by_error;
DROP TABLE test.create_select;
