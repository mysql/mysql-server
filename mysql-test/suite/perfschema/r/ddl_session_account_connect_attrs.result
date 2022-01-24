ALTER TABLE performance_schema.session_account_connect_attrs
ADD COLUMN foo INTEGER;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.session_account_connect_attrs;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'session_account_connect_attrs'
ALTER TABLE performance_schema.session_account_connect_attrs ADD INDEX test_index(ATTR_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.session_account_connect_attrs(ATTR_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.session_account_connect_attrs;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.session_account_connect_attrs;
DROP TABLE test.create_select;
