#
# Bug#15923864 (Bug#67718):
#  INNODB DRASTICALLY UNDER-FILLS PAGES IN CERTAIN CONDITIONS
#
SET GLOBAL innodb_file_per_table=ON;
CREATE TABLE t1 (a BIGINT PRIMARY KEY, b VARCHAR(4096)) ENGINE=InnoDB;
INSERT INTO t1 VALUES (0, REPEAT('a', 4096));
INSERT INTO t1 VALUES (1000, REPEAT('a', 4096));
INSERT INTO t1 VALUES (1001, REPEAT('a', 4096));
INSERT INTO t1 VALUES (1002, REPEAT('a', 4096));
INSERT INTO t1 VALUES (1, REPEAT('a', 4096));
INSERT INTO t1 VALUES (2, REPEAT('a', 4096));
SELECT page_number, number_records
FROM information_schema.innodb_tablespaces s1,
information_schema.innodb_buffer_page s2,
information_schema.innodb_tables s3
WHERE s1.space = s2.space AND s3.name = 'test/t1'
AND page_type = "INDEX" AND s1.space = s3.space
ORDER BY page_number;
page_number	number_records
4	2
5	3
6	3
INSERT INTO t1 VALUES (999, REPEAT('a', 4096));
SELECT page_number, number_records
FROM information_schema.innodb_tablespaces s1,
information_schema.innodb_buffer_page s2,
information_schema.innodb_tables s3
WHERE s1.space = s2.space AND s3.name = 'test/t1'
AND page_type = "INDEX" AND s1.space = s3.space
ORDER BY page_number;
page_number	number_records
4	3
5	3
6	3
7	1
INSERT INTO t1 VALUES (998, REPEAT('a', 4096));
SELECT page_number, number_records
FROM information_schema.innodb_tablespaces s1,
information_schema.innodb_buffer_page s2,
information_schema.innodb_tables s3
WHERE s1.space = s2.space AND s3.name = 'test/t1'
AND page_type = "INDEX" AND s1.space = s3.space
ORDER BY page_number;
page_number	number_records
4	3
5	3
6	3
7	2
INSERT INTO t1 VALUES (997, REPEAT('a', 4096));
SELECT page_number, number_records
FROM information_schema.innodb_tablespaces s1,
information_schema.innodb_buffer_page s2,
information_schema.innodb_tables s3
WHERE s1.space = s2.space AND s3.name = 'test/t1'
AND page_type = "INDEX" AND s1.space = s3.space
ORDER BY page_number;
page_number	number_records
4	3
5	3
6	3
7	3
DROP TABLE t1;
