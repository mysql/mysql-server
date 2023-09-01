#
# Test of alter table
#
--disable_warnings
drop table if exists t1;
--enable_warnings

CREATE TABLE t1 SELECT _utf8mb3'test' as c1, _utf8mb3'тест' as c2;
SHOW CREATE TABLE t1;
DELETE FROM t1;
ALTER TABLE t1 ADD c3 CHAR(4) CHARACTER SET utf8mb3;
SHOW CREATE TABLE t1;
INSERT IGNORE INTO t1 VALUES ('aaaabbbbccccdddd','aaaabbbbccccdddd','aaaabbbbccccdddd');
SELECT * FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (a CHAR(4) CHARACTER SET utf8mb3, KEY key_a(a(3)));
SHOW CREATE TABLE t1;
ANALYZE TABLE t1;
SHOW KEYS FROM t1;
ALTER TABLE t1 CHANGE a a CHAR(4);
SHOW CREATE TABLE t1;
ANALYZE TABLE t1;
SHOW KEYS FROM t1;
ALTER TABLE t1 CHANGE a a CHAR(4) CHARACTER SET utf8mb3;
SHOW CREATE TABLE t1;
ANALYZE TABLE t1;
SHOW KEYS FROM t1;
DROP TABLE t1;

# End of 4.1 tests
