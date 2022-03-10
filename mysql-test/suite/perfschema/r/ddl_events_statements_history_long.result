alter table performance_schema.events_statements_history_long add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_statements_history_long;
ALTER TABLE performance_schema.events_statements_history_long ADD INDEX test_index(EVENT_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.events_statements_history_long(EVENT_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.events_statements_history_long;
DROP TABLE test.create_select;
