ALTER TABLE performance_schema.replication_applier_global_filters
ADD COLUMN foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_applier_global_filters;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_applier_global_filters'
ALTER TABLE performance_schema.replication_applier_global_filters
ADD INDEX test_index(filter_name);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON
performance_schema.replication_global_applier_filters(filter_name);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
