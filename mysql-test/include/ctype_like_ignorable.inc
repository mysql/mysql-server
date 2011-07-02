SELECT @@collation_connection;
--echo #
--echo # Bug#57737 Character sets: search fails with like, contraction, index
--echo # Part#2 - ignorable characters
--echo #
CREATE TABLE t1 AS SELECT REPEAT(' ', 10) AS s1 LIMIT 0;
INSERT INTO t1 VALUES ('a\0\0\0\0\0\t'),('a'),('b'),('c'),('d'),('e');
SELECT HEX(s1) FROM t1 WHERE s1 LIKE 'a%';
ALTER TABLE t1 ADD KEY s1 (s1);
SELECT HEX(s1) FROM t1 WHERE s1 LIKE 'a%';
DROP TABLE t1;
