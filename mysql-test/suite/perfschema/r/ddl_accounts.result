alter table performance_schema.accounts
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.accounts;
ALTER TABLE performance_schema.accounts ADD INDEX test_index(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.accounts(CURRENT_CONNECTIONS);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY` ON performance_schema.accounts;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `ACCOUNT`
  ON performance_schema.accounts;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.accounts;
DROP TABLE test.create_select;
