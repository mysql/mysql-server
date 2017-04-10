alter table performance_schema.events_transactions_history add column foo integer;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
truncate table performance_schema.events_transactions_history;
ALTER TABLE performance_schema.events_transactions_history ADD INDEX test_index(EVENT_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.events_transactions_history(EVENT_ID);
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY`
  ON performance_schema.events_transactions_history;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
