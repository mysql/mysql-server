SET GLOBAL innodb_buffer_pool_dump_pct=100;
CREATE TABLE ib_bp_test
(a INT AUTO_INCREMENT, b VARCHAR(64), c TEXT, PRIMARY KEY (a), KEY (b, c(128)))
ENGINE=INNODB;
SELECT COUNT(*) FROM information_schema.innodb_buffer_page_lru
WHERE table_name LIKE '%ib_bp_test%';
COUNT(*)
2
SELECT COUNT(*) FROM information_schema.innodb_buffer_page_lru
WHERE table_name LIKE '%ib_bp_test%';
COUNT(*)
{checked_valid}
# restart
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool load completed or still running
SELECT COUNT(*) FROM information_schema.innodb_buffer_page_lru
WHERE table_name LIKE '%ib_bp_test%';
COUNT(*)
0
call mtr.add_suppression("Error parsing");
# restart
# Re-write some valid pages to the dump file, make sure the space
# should be valid but all the page no should be out of bound of the file
# restart
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool load completed or still running
DROP TABLE ib_bp_test;
SET GLOBAL innodb_buffer_pool_dump_pct=default;
#
# Bug#22016556 INNODB LOOKS FOR BUFFER POOL FILE NAME IN '/' IF
# INNODB_DATA_HOME_DIR IS EMPTY
#
# restart: --innodb-data-home-dir= --innodb_data_file_path=MYSQL_TMP_DIR/datadir1/ibdata:16M:autoextend --datadir=MYSQLD_DATADIR1 --innodb_log_file_size=15M --innodb_log_files_in_group=2
CREATE TABLE t1 (a int) ENGINE=innodb;
INSERT INTO t1 VALUES (1), (2), (3);
SET GLOBAL innodb_buffer_pool_dump_now=ON;
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_dump_status';
variable_value
Buffer pool(s) dump completed at TIMESTAMP_NOW
SET GLOBAL innodb_buffer_pool_load_now = ON;
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool load completed or still running
DROP TABLE t1;
DROP DATABASE test;
# restart
