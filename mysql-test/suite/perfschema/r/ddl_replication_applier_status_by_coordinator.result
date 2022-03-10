ALTER TABLE performance_schema.replication_applier_status_by_coordinator
ADD COLUMN foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_applier_status_by_coordinator;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_applier_status_by_coordinator'
ALTER TABLE performance_schema.replication_applier_status_by_coordinator
ADD INDEX test_index(thread_id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON
performance_schema.replication_applier_status_by_coordinator(thread_id);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.replication_applier_status_by_coordinator;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.replication_applier_status_by_coordinator;
DROP TABLE test.create_select;
