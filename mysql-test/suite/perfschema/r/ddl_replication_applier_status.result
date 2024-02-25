ALTER TABLE performance_schema.replication_applier_status
ADD COLUMN foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_applier_status;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_applier_status'
ALTER TABLE performance_schema.replication_applier_status
ADD INDEX test_index(remaining_delay);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON
performance_schema.replication_applier_status(remaining_delay);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.replication_applier_status;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.replication_applier_status;
DROP TABLE test.create_select;
