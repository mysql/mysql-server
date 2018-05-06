alter table performance_schema.table_io_waits_summary_by_index_usage
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.table_io_waits_summary_by_index_usage;
ALTER TABLE performance_schema.table_io_waits_summary_by_index_usage ADD INDEX test_index(TABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.table_io_waits_summary_by_index_usage(TABLE_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `OBJECT`
  ON performance_schema.table_io_waits_summary_by_index_usage;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
