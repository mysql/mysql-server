ALTER TABLE performance_schema.keyring_keys
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.keyring_keys;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'keyring_keys'
ALTER TABLE performance_schema.keyring_keys ADD INDEX test_index(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.keyring_keys(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.keyring_keys;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
