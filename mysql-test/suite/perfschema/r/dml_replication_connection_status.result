SELECT * FROM performance_schema.replication_connection_status
LIMIT 1;
SELECT * FROM performance_schema.replication_connection_status
WHERE received_transaction_set='FOO' OR last_error_message='BAR';
INSERT INTO performance_schema.replication_connection_status
SET received_transaction_set='FOO', last_error_message='BAR',
thread_id=1, last_error_number=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_connection_status'
UPDATE performance_schema.replication_connection_status
SET last_error_number=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_connection_status'
UPDATE performance_schema.replication_connection_status
SET thread_id=12 WHERE received_transaction_set like "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_connection_status'
DELETE FROM performance_schema.replication_connection_status
WHERE last_error_number=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_connection_status'
DELETE FROM performance_schema.replication_connection_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_connection_status'
LOCK TABLES performance_schema.replication_connection_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_connection_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_connection_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_connection_status'
UNLOCK TABLES;
