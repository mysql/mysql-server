alter table performance_schema.events_statements_summary_by_program
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_statements_summary_by_program;
ALTER TABLE performance_schema.events_statements_summary_by_program ADD INDEX test_index(OBJECT_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_statements_summary_by_program(OBJECT_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.events_statements_summary_by_program;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
