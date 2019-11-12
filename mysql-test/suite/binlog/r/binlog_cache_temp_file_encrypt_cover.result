CREATE TABLE t1 (c1 TEXT);
# Adding debug point 'verify_mysql_encryption_file_write_bytes' to @@GLOBAL.debug
INSERT INTO t1 VALUES (REPEAT('123', 16384.0));
# Removing debug point 'verify_mysql_encryption_file_write_bytes' from @@GLOBAL.debug
# Adding debug point 'simulate_binlog_cache_temp_file_encrypt_fail' to @@GLOBAL.debug
INSERT INTO t1 VALUES (REPEAT('456', 16384.0));
ERROR HY000: Error writing file 'binlog' ((errno: #)
# Removing debug point 'simulate_binlog_cache_temp_file_encrypt_fail' from @@GLOBAL.debug
DROP TABLE t1;
