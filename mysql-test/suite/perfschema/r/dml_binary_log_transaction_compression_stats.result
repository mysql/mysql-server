SELECT * FROM performance_schema.binary_log_transaction_compression_stats LIMIT 1;
SELECT * FROM performance_schema.binary_log_transaction_compression_stats WHERE log_type='BINARY' OR COMPRESSION_TYPE='LZ4';
INSERT INTO performance_schema.binary_log_transaction_compression_stats SET log_type='BINARY', compression_type='LZ4', transaction_counter=1, compressed_bytes_counter=2, uncompressed_bytes_counter=1, last_transaction_id="gtid", last_transaction_compressed_bytes=1, last_transaction_uncompressed_bytes=1, las_transaction_timestamp=now();
ERROR 42000: INSERT command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
UPDATE performance_schema.binary_log_transaction_compression_stats SET log_type='BINARY';
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
UPDATE performance_schema.binary_log_transaction_compression_stats SET log_type='BINARY' WHERE compression_type="LZ4";
ERROR 42000: UPDATE command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
DELETE FROM performance_schema.binary_log_transaction_compression_stats WHERE log_type='BINARY';
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
DELETE FROM performance_schema.binary_log_transaction_compression_stats;
ERROR 42000: DELETE command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
LOCK TABLES performance_schema.binary_log_transaction_compression_stats READ;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
UNLOCK TABLES;
LOCK TABLES performance_schema.binary_log_transaction_compression_stats WRITE;
ERROR 42000: SELECT, LOCK TABLES command denied to user 'root'@'localhost' for table 'binary_log_transaction_compression_stats'
UNLOCK TABLES;
