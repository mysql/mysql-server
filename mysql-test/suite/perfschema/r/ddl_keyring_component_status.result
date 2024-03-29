ALTER TABLE performance_schema.keyring_component_status
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.keyring_component_status;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'keyring_component_status'
ALTER TABLE performance_schema.keyring_component_status ADD INDEX test_index(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.keyring_component_status(KEY_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.keyring_component_status;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
call mtr.add_suppression("No suitable 'keyring_component_metadata_query' service implementation found to fulfill the request");
CREATE TABLE test.create_select
AS SELECT * from performance_schema.keyring_component_status;
DROP TABLE test.create_select;
