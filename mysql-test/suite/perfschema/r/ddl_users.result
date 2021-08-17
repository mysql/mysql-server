alter table performance_schema.users
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.users;
ALTER TABLE performance_schema.users ADD INDEX test_index(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.users(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `USER`
  ON performance_schema.users;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.users;
DROP TABLE test.create_select;
