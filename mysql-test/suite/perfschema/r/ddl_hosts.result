alter table performance_schema.hosts
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.hosts;
ALTER TABLE performance_schema.hosts ADD INDEX test_index(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.hosts(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `HOST`
  ON performance_schema.hosts;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.hosts;
DROP TABLE test.create_select;
