SET SESSION binlog_transaction_compression = ON;
CREATE TABLE t1 (c1 INT UNSIGNED PRIMARY KEY, c2 LONGTEXT);
INSERT INTO t1 VALUES (2, REPEAT('b', 1000000));
DROP TABLE t1;
include/assert.inc [Number of rows in performance_schema.binary_log_transaction_compression_stats = 2]
# restart
include/assert.inc [Number of rows in performance_schema.binary_log_transaction_compression_stats = 0]
# restart:--skip-log-bin
SET SESSION binlog_transaction_compression = ON;
CREATE TABLE t1 (c1 INT UNSIGNED PRIMARY KEY, c2 LONGTEXT);
INSERT INTO t1 VALUES (2, REPEAT('b', 1000000));
DROP TABLE t1;
include/assert.inc [Number of rows in performance_schema.binary_log_transaction_compression_stats = 0]
# restart
SET SESSION binlog_transaction_compression = 0;
SET GLOBAL binlog_transaction_compression = 0;
