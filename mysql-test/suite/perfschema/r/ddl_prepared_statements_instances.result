alter table performance_schema.prepared_statements_instances
add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.prepared_statements_instances;
ALTER TABLE performance_schema.prepared_statements_instances ADD INDEX test_index(OWNER_OBJECT_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index
ON performance_schema.prepared_statements_instances(OWNER_OBJECT_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.prepared_statements_instances;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.prepared_statements_instances;
DROP TABLE test.create_select;
