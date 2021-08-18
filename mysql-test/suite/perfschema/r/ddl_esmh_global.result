alter table performance_schema.events_statements_histogram_global
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_statements_histogram_global;
ALTER TABLE performance_schema.events_statements_histogram_global ADD INDEX test_index(DIGEST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_statements_histogram_global(DIGEST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `SCHEMA_NAME`
  ON performance_schema.events_statements_histogram_global;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.events_statements_histogram_global;
Warnings:
Warning	1681	Specifying number of digits for floating point data types is deprecated and will be removed in a future release.
DROP TABLE test.create_select;
