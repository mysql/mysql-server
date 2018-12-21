SELECT * FROM performance_schema.keyring_keys
LIMIT 1;
SELECT * FROM performance_schema.keyring_keys
where KEY_ID='FOO' OR KEY_OWNER='BAR';
INSERT INTO performance_schema.keyring_keys
SET KEY_ID='FOO', KEY_OWNER='BAR';
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'keyring_keys'
UPDATE performance_schema.keyring_keys
SET KEY_ID='FOO';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'keyring_keys'
UPDATE performance_schema.keyring_keys
SET KEY_ID='FOO' WHERE KEY_OWNER='BAR';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'keyring_keys'
DELETE FROM performance_schema.keyring_keys
WHERE KEY_OWNER='BAR';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'keyring_keys'
DELETE FROM performance_schema.keyring_keys;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'keyring_keys'
LOCK TABLES performance_schema.keyring_keys READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'keyring_keys'
UNLOCK TABLES;
LOCK TABLES performance_schema.keyring_keys WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'keyring_keys'
UNLOCK TABLES;
