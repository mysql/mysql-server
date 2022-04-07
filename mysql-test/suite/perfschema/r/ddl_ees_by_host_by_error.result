alter table performance_schema.events_errors_summary_by_host_by_error
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_errors_summary_by_host_by_error;
ALTER TABLE performance_schema.events_errors_summary_by_host_by_error
ADD INDEX test_index(HOSTNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_errors_summary_by_host_by_error(HOSTNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `HOST`
  ON performance_schema.events_errors_summary_by_host_by_error;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
