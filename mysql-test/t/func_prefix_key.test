CREATE TABLE t1(a int, b VARCHAR(5), PRIMARY KEY(a))ENGINE=INNODB;
INSERT INTO t1 VALUES (1, 'a'), (2, 'ab'), (3, 'abc'), (4, 'abcd'), (5, 'abcde');
ALTER TABLE t1 ADD KEY k2 (b(4));
ANALYZE TABLE t1;

--source include/func_prefix_key.inc

DROP TABLE t1;

CREATE TABLE t1(a int, b VARCHAR(5), PRIMARY KEY(a))ENGINE=INNODB  DEFAULT CHARSET=latin1;
INSERT INTO t1 VALUES (1, 'a'), (2, 'ab'), (3, 'abc'), (4, 'abcd'), (5, 'abcde');
ALTER TABLE t1 ADD KEY k2 (b(4));
ANALYZE TABLE t1;

--source include/func_prefix_key.inc

DROP TABLE t1;

CREATE TABLE t1(a int, b VARCHAR(5), PRIMARY KEY(a))ENGINE=INNODB  DEFAULT CHARSET=utf32;
INSERT INTO t1 VALUES (1, 'a'), (2, 'ab'), (3, 'abc'), (4, 'abcd'), (5, 'abcde');
ALTER TABLE t1 ADD KEY k2 (b(4));
ANALYZE TABLE t1;

--source include/func_prefix_key.inc

DROP TABLE t1;

# Test for TEXT field.

CREATE TABLE t1(a int, b TEXT, PRIMARY KEY(a))ENGINE=INNODB;
INSERT INTO t1 VALUES (1, 'a'), (2, 'ab'), (3, 'abc'), (4, 'abcd'), (5, 'abcde');
ALTER TABLE t1 ADD KEY k2 (b(4));
ANALYZE TABLE t1;

--source include/func_prefix_key.inc

DROP TABLE t1;

# More tests with the indexes with different prefix length.

CREATE TABLE t1(a int, b VARCHAR(5), PRIMARY KEY(a))ENGINE=INNODB  DEFAULT CHARSET=utf32;
INSERT INTO t1 VALUES (1, 'a'), (2, 'ab'), (3, 'abc'), (4, 'abcd'), (5, 'abcde'), (6, 'abcdf');
ALTER TABLE t1 ADD KEY k2 (b(4));
ALTER TABLE t1 ADD KEY k3 (b(2));
ALTER TABLE t1 ADD KEY k4 (a, b(3));
ANALYZE TABLE t1;

--echo # k2 index used since it's  covering.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'abc%';
SELECT COUNT(*) FROM t1 WHERE b like 'abc%';

--echo # k4 index used since it's  covering.
ANALYZE TABLE t1;
#Adding hint to stabilize the test
EXPLAIN SELECT /*+ INDEX(t1 k4) */ COUNT(*) FROM t1 WHERE a > 4 AND b like 'abc%';
SELECT /*+ INDEX(t1 k4) */ COUNT(*) FROM t1 WHERE a > 4 AND b like 'abc%';

--echo # k3 index is used since it's shorters key.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'ab%';
SELECT COUNT(*) FROM t1 WHERE b like 'ab%';

--echo # Index access is not used, no covering keys.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE a > 4 AND b like 'abcde%';
SELECT COUNT(*) FROM t1 WHERE a > 4 AND b like 'abcde%';

--echo # k2 index used since it's  covering.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE a > 4 AND b like 'abcd%';
SELECT COUNT(*) FROM t1 WHERE a > 4 AND b like 'abcd%';

--echo # Index access is not used, no covering keys.
EXPLAIN SELECT b like 'abcdf%'  FROM t1 WHERE a > 4 AND b like 'abcd%';
SELECT b like 'abcdf%'  FROM t1 WHERE a > 4 AND b like 'abcd%';

--echo # Index access is used since k4 is covering.
EXPLAIN SELECT b like 'ab%'  FROM t1 WHERE a > 4 AND b like 'a%';
SELECT b like 'ab%'  FROM t1 WHERE a > 4 AND b like 'a%';

--echo # Index access is used since k3 is forced and covering.
EXPLAIN SELECT b like 'ab%'  FROM t1 FORCE INDEX(k3) WHERE a > 4 AND b like 'a%';
SELECT b like 'ab%'  FROM t1 FORCE INDEX(k3) WHERE a > 4 AND b like 'a%';

--echo # No index access is used since second argument is not const.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like a;
SELECT COUNT(*) FROM t1 WHERE b like a;

DROP TABLE t1;

CREATE TABLE t1(a int, b TEXT, c TEXT, PRIMARY KEY(a))ENGINE=INNODB;
INSERT INTO t1 VALUES (1, 'a', 'a'), (2, 'ab', 'ab'), (3, 'abc', 'abc'), (4, 'abcd', 'abcd'), (5, 'abcde', 'abcde');
ALTER TABLE t1 ADD KEY k2 (b(4), c(3));
ANALYZE TABLE t1;
--echo # Index access is used since key is covering.
EXPLAIN SELECT COUNT(*) FROM t1 WHERE b like 'aaaa';
DROP TABLE t1;

--echo #
--echo # Bug#27660560 RECENT REGRESSION: CRASH IN CHECK_COVERING_PREFIX_KEYS.
--echo #

CREATE TABLE t1(f1 BLOB, KEY(f1(1))) ENGINE=INNODB;
INSERT INTO t1 VALUES ('ccc'), ('aa');
ANALYZE TABLE t1;

SELECT (f1 LIKE null) from t1;
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT 1 FROM t1 WHERE f1 NOT LIKE json_merge('' ,'+' );
--error ER_INVALID_JSON_TEXT_IN_PARAM
SELECT 1 FROM t1 WHERE f1 LIKE json_contains('key2' ,'key4' );
SELECT 1 FROM t1 WHERE f1 LIKE json_depth(null);

# Tests below should use index access.
EXPLAIN SELECT (f1 LIKE null) from t1;
EXPLAIN SELECT (f1 LIKE null) from t1 WHERE f1 LIKE 'a%';

DROP TABLE t1;

--echo #
--echo # Bug#28934315 CASE STATEMENT USE IN CONDITIONAL WITH SUB_PART INDEX REGRESSION IN 8.0.
--echo #

CREATE TABLE t1 (
  id INT(11) NOT NULL AUTO_INCREMENT,
  example VARCHAR(100) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  KEY example_key (example(9))
) ENGINE=InnoDB;

CREATE TABLE t2 (
  id INT(11) NOT NULL AUTO_INCREMENT,
  example VARCHAR(100) NOT NULL DEFAULT '',
  PRIMARY KEY (id)
) ENGINE=InnoDB;

INSERT INTO t1 (example) VALUES ('1234567890');
INSERT INTO t2 (example) VALUES ('1234567890');

EXPLAIN SELECT t2.example, t2.id FROM t2, t1 WHERE t1.example = LOWER(t2.example);
SELECT t2.example, t2.id FROM t2, t1 WHERE t1.example = LOWER(t2.example);

DROP TABLE t1, t2;
