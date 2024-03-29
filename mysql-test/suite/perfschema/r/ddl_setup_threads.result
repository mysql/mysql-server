alter table performance_schema.setup_threads add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.setup_threads;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'setup_threads'
ALTER TABLE performance_schema.setup_threads ADD INDEX test_index(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.setup_threads(NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY` ON performance_schema.setup_threads;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.setup_threads;
DROP TABLE test.create_select;
