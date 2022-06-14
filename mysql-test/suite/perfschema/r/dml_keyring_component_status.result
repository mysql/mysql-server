SELECT * FROM performance_schema.keyring_component_status
LIMIT 1;
SELECT * FROM performance_schema.keyring_component_status
where STATUS_KEY ='FOO' OR STATUS_VALUE ='BAR';
INSERT INTO performance_schema.keyring_component_status
SET CHANNEL='FOO', PROPERTY='BAR', VALUE='BAZ';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'keyring_component_status'
UPDATE performance_schema.keyring_component_status
SET STATUS_KEY='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'keyring_component_status'
UPDATE performance_schema.keyring_component_status
SET STATUS_KEY ='FOO' OR STATUS_VALUE ='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'keyring_component_status'
DELETE FROM performance_schema.keyring_component_status
WHERE STATUS_VALUE='BAR';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'keyring_component_status'
DELETE FROM performance_schema.keyring_component_status;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'keyring_component_status'
LOCK TABLES performance_schema.keyring_component_status READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'keyring_component_status'
UNLOCK TABLES;
LOCK TABLES performance_schema.keyring_component_status WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'keyring_component_status'
UNLOCK TABLES;
