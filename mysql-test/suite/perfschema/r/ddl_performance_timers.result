alter table performance_schema.performance_timers add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.performance_timers;
ERROR 42000: DROP command denied to user 'root'@'localhost' for table 'performance_timers'
ALTER TABLE performance_schema.performance_timers ADD INDEX test_index(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.performance_timers(TIMER_NAME);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select
AS SELECT * from performance_schema.performance_timers;
DROP TABLE test.create_select;
