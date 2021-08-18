ALTER TABLE performance_schema.replication_connection_configuration
ADD COLUMN foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.replication_connection_configuration;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'replication_connection_configuration'
ALTER TABLE performance_schema.replication_connection_configuration
ADD INDEX test_index(HOST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.replication_connection_configuration(HOST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.replication_connection_configuration;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.replication_connection_configuration;
Warnings:
Warning	1681	Specifying number of digits for floating point data types is deprecated and will be removed in a future release.
DROP TABLE test.create_select;
