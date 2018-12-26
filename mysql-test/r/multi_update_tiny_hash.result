drop table if exists t1, t2;
#
# Bug #36676: multiupdate using LEFT JOIN updates only
#             first row or fails with an error:
#  ERROR 1022 (23000): Can't write; duplicate key in table ''
#

#
# Multiupdate creates MyISAM temporary table without MEMORY table
#
CREATE TABLE t1 (ID INT);
CREATE TABLE t2 (ID INT,
s1 TEXT, s2 TEXT, s3 VARCHAR(10), s4 TEXT, s5 VARCHAR(10));
INSERT INTO t1 VALUES (1),(2);
INSERT INTO t2 VALUES (1,'test', 'test', 'test', 'test', 'test'),
(2,'test', 'test', 'test', 'test', 'test');
SELECT * FROM t1 LEFT JOIN t2 USING(ID);
ID	s1	s2	s3	s4	s5
1	test	test	test	test	test
2	test	test	test	test	test
UPDATE t1 LEFT JOIN t2 USING(ID) SET s1 = 'changed';
UPDATE t1 JOIN t2 USING(ID) SET s2 = 'changed';
UPDATE t1 LEFT JOIN t2 USING(ID) SET s3 = 'changed';
UPDATE t1 LEFT JOIN t2 USING(ID) SET s4 = 'changed', s5 = 'changed';
SELECT * FROM t1 LEFT JOIN t2 USING(ID);
ID	s1	s2	s3	s4	s5
1	changed	changed	changed	changed	changed
2	changed	changed	changed	changed	changed
DROP TABLE t1, t2;
#
# Multiupdate creates temporary MyISAM table from MEMORY table
#
CREATE TABLE t1 (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY);
CREATE TABLE t2 (id INT, s1 CHAR(255));
INSERT INTO t1 VALUES (0), (0), (0), (0), (0), (0), (0), (0);
INSERT INTO t1 (SELECT 0 FROM t1);
INSERT INTO t1 (SELECT 0 FROM t1);
INSERT INTO t1 (SELECT 0 FROM t1);
INSERT INTO t2 (SELECT ID, 'a' FROM t1);
UPDATE t1 LEFT JOIN t2 USING(id) SET s1 = 'b';
SELECT DISTINCT s1 FROM t1 LEFT JOIN t2 USING(id);
s1
b
DROP TABLE t1, t2;
# End of 5.0 tests
