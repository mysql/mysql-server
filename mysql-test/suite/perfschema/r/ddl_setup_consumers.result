alter table performance_schema.setup_consumers add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.setup_consumers;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'setup_consumers'
ALTER TABLE performance_schema.setup_consumers ADD INDEX test_index(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.setup_consumers(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.setup_consumers;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
