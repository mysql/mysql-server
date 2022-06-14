SELECT * FROM performance_schema.tls_channel_status
LIMIT 1;
SELECT * FROM performance_schema.tls_channel_status
where CHANNEL ='FOO' OR PROPERTY='BAR';
INSERT INTO performance_schema.tls_channel_status
SET CHANNEL='FOO', PROPERTY='BAR', VALUE='BAZ';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'tls_channel_status'
UPDATE performance_schema.tls_channel_status
SET CHANNEL='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'tls_channel_status'
UPDATE performance_schema.tls_channel_status
SET CHANNEL='FOO' WHERE PROPERTY='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'tls_channel_status'
DELETE FROM performance_schema.tls_channel_status
WHERE VALUE='BAR';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'tls_channel_status'
DELETE FROM performance_schema.tls_channel_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'tls_channel_status'
LOCK TABLES performance_schema.tls_channel_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'tls_channel_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.tls_channel_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'tls_channel_status'
UNLOCK TABLES;
