ALTER TABLE performance_schema.binary_log_transaction_compression_stats add column foo integer;;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
TRUNCATE TABLE performance_schema.binary_log_transaction_compression_stats;
ALTER TABLE performance_schema.binary_log_transaction_compression_stats ADD INDEX test_index(last_transaction_id);;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE UNIQUE INDEX test_index ON performance_schema.binary_log_transaction_compression_stats(last_transaction_id);;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
DROP INDEX `PRIMARY` ON performance_schema.binary_log_transaction_compression_stats;
ERROR 42000: Access denied for user 'root'@'localhost' to database 'performance_schema'
CREATE TABLE test.create_select AS SELECT * from performance_schema.binary_log_transaction_compression_stats;;
DROP TABLE test.create_select;
