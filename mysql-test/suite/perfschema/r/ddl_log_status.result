ALTER TABLE performance_schema.log_status
ADD COLUMN foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.log_status;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'log_status'
ALTER TABLE performance_schema.log_status
ADD INDEX test_index(server_uuid);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON
performance_schema.log_status(server_uuid);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
