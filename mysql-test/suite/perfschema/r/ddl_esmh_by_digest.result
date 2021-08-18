alter table performance_schema.events_statements_histogram_by_digest
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_statements_histogram_by_digest;
ALTER TABLE performance_schema.events_statements_histogram_by_digest ADD INDEX test_index(DIGEST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.events_statements_histogram_by_digest(DIGEST);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `SCHEMA_NAME`
  ON performance_schema.events_statements_histogram_by_digest;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.events_statements_histogram_by_digest;
Warnings:
Warning	1681	Specifying number of digits for floating point data types is deprecated and will be removed in a future release.
DROP TABLE test.create_select;
