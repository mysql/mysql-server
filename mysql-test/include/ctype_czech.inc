SELECT @@collation_connection;
--echo #
--echo # Bug#57737 Character sets: search fails with like, contraction, index
--echo #
CREATE TABLE t1 AS SELECT REPEAT(' ', 10) AS s1 LIMIT 0;
INSERT INTO t1 VALUES ('c'),('ce'),('cé'),('ch');
SELECT * FROM t1 WHERE s1 LIKE 'c%';
ALTER TABLE t1 ADD KEY s1 (s1);
SELECT * FROM t1 WHERE s1 LIKE 'c%';
ALTER TABLE t1 DROP KEY s1, ADD KEY(s1(1));
SELECT * FROM t1 WHERE s1 LIKE 'ch';
DROP TABLE t1;
