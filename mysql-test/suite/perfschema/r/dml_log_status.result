SELECT * FROM performance_schema.log_status
LIMIT 1;
INSERT INTO performance_schema.log_status
SET server_uuid='ABC123';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'log_status'
UPDATE performance_schema.log_status
SET server_uuid='ABC123' WHERE server_uuid=@@server_uuid;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'log_status'
DELETE FROM performance_schema.log_status
WHERE server_uuid=@@server_uuid;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'log_status'
DELETE FROM performance_schema.log_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'log_status'
LOCK TABLES performance_schema.log_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'log_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.log_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'log_status'
UNLOCK TABLES;
