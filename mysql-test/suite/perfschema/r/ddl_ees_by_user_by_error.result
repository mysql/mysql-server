alter table performance_schema.events_errors_summary_by_user_by_error
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_errors_summary_by_user_by_error;
ALTER TABLE performance_schema.events_errors_summary_by_user_by_error
ADD INDEX test_index(USERNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_errors_summary_by_user_by_error(USERNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `USER`
  ON performance_schema.events_errors_summary_by_user_by_error;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
