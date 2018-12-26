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
SET GLOBAL innodb_buffer_pool_dump_now = ON;
# restart
SELECT COUNT(*) FROM information_schema.innodb_buffer_page_lru
WHERE table_name LIKE '%ib_bp_test%';
COUNT(*)
0
select count(*) from ib_bp_test where a = 1;
count(*)
1
SET GLOBAL innodb_buffer_pool_load_now = ON;
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool(s) load completed at TIMESTAMP_NOW
SELECT COUNT(*) FROM information_schema.innodb_buffer_page_lru
WHERE table_name LIKE '%ib_bp_test%';
COUNT(*)
{checked_valid}
call mtr.add_suppression("Error parsing");
SET GLOBAL innodb_buffer_pool_load_now = ON;
# Re-write some valid pages to the dump file, make sure the space
# should be valid but all the page no should be out of bound of the file
# restart
SET GLOBAL innodb_buffer_pool_load_now = ON;
SELECT variable_value
FROM performance_schema.global_status
WHERE LOWER(variable_name) = 'innodb_buffer_pool_load_status';
variable_value
Buffer pool(s) load completed at TIMESTAMP_NOW
DROP TABLE ib_bp_test;
SET GLOBAL innodb_buffer_pool_dump_pct=default;
#
# Bug#21371070 [ERROR] INNODB: CANNOT ALLOCATE 0 BYTES: SUCCESS
#
SET GLOBAL innodb_buffer_pool_load_now = ON;
