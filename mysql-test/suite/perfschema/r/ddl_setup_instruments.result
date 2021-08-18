alter table performance_schema.setup_instruments add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.setup_instruments;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'setup_instruments'
ALTER TABLE performance_schema.setup_instruments ADD INDEX test_index(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.setup_instruments(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.setup_instruments;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.setup_instruments;
DROP TABLE test.create_select;
