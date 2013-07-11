SELECT * FROM performance_schema.replication_connection_configuration
LIMIT 1;
SELECT * FROM performance_schema.replication_connection_configuration
WHERE user='FOO' or host='BAR';
INSERT INTO performance_schema.replication_connection_configuration
SET user='FOO', host='BAR',
port=1, connection_retry_count=2;
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
UPDATE performance_schema.replication_connection_configuration
SET connection_retry_interval=12;
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
UPDATE performance_schema.replication_connection_configuration
SET connection_retry_interval=12 WHERE host LIKE "FOO";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
DELETE FROM performance_schema.replication_connection_configuration
WHERE connection_retry_count=1;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
DELETE FROM performance_schema.replication_connection_configuration;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
LOCK TABLES performance_schema.replication_connection_configuration READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
UNLOCK TABLES;
LOCK TABLES performance_schema.replication_connection_configuration WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
UNLOCK TABLES;
