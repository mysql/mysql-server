alter table performance_schema.events_stages_summary_by_host_by_event_name
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_stages_summary_by_host_by_event_name;
ALTER TABLE performance_schema.events_stages_summary_by_host_by_event_name
ADD INDEX test_index(HOSTNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_stages_summary_by_host_by_event_name(HOSTNAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `HOST`
  ON performance_schema.events_stages_summary_by_host_by_event_name;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
