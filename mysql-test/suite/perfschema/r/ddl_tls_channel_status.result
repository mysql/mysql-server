ALTER TABLE performance_schema.tls_channel_status
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.tls_channel_status;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'tls_channel_status'
ALTER TABLE performance_schema.tls_channel_status ADD INDEX test_index(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.tls_channel_status(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.tls_channel_status;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.tls_channel_status;
DROP TABLE test.create_select;
