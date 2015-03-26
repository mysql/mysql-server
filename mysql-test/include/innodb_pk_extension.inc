--source include/have_innodb.inc

CREATE TABLE t1
(
 pk_1 INT,
 pk_2 INT,
 f1 DATETIME,
 f2 INT,
 PRIMARY KEY(pk_1, pk_2),
 KEY k1(f1),
 KEY k2(f2)
) ENGINE = InnoDB;

INSERT INTO t1 VALUES
(1, 1, '2000-01-01', 1), (1, 2, '2000-01-02', 2), (1, 3, '2000-01-03', 3), (1, 4, '2000-01-04', 4), (1, 5, '2000-01-05', 5),
(2, 1, '2000-01-01', 6), (2, 2, '2000-01-02', 7), (2, 3, '2000-01-03', 8), (2, 4, '2000-01-04', 9), (2, 5, '2000-01-05', 10),
(3, 1, '2000-01-01', 11), (3, 2, '2000-01-02', 12), (3, 3, '2000-01-03', 13), (3, 4, '2000-01-04', 14), (3, 5, '2000-01-05', 15),
(4, 1, '2000-01-01', 16), (4, 2, '2000-01-02', 17), (4, 3, '2000-01-03', 18), (4, 4, '2000-01-04', 19), (4, 5, '2000-01-05', 20),
(5, 1, '2000-01-01', 21), (5, 2, '2000-01-02', 22), (5, 3, '2000-01-03', 23), (5, 4, '2000-01-04', 24), (5, 5, '2000-01-05', 25),
(6, 1, '2000-01-06', 26), (6, 2, '2000-01-06', 27), (6, 3, '2000-01-03', 28), (6, 4, '2000-01-06', 29), (6, 5, '2000-01-06', 30),
(7, 1, '2000-01-06', 31), (7, 2, '2000-01-06', 32), (7, 3, '2000-01-03', 33), (7, 4, '2000-01-06', 34), (7, 5, '2000-01-06', 35),
(8, 1, '2000-01-06', 36), (8, 2, '2000-01-06', 37), (8, 3, '2000-01-03', 38), (8, 4, '2000-01-06', 39), (8, 5, '2000-01-06', 40),
(9, 1, '2000-01-06', 41), (9, 2, '2000-01-06', 42), (9, 3, '2000-01-03', 43), (9, 4, '2000-01-06', 44), (9, 5, '2000-01-06', 45);

INSERT INTO t1 VALUES
(11, 1, '2000-01-01', 1), (11, 2, '2000-01-02', 2), (11, 3, '2000-01-03', 3), (11, 4, '2000-01-04', 4), (11, 5, '2000-01-05', 5),
(12, 1, '2000-01-01', 6), (12, 2, '2000-01-02', 7), (12, 3, '2000-01-03', 8), (12, 4, '2000-01-04', 9), (12, 5, '2000-01-05', 10),
(13, 1, '2000-01-01', 11), (13, 2, '2000-01-02', 12), (13, 3, '2000-01-03', 13), (13, 4, '2000-01-04', 14), (13, 5, '2000-01-05', 15),
(14, 1, '2000-01-01', 16), (14, 2, '2000-01-02', 17), (14, 3, '2000-01-03', 18), (14, 4, '2000-01-04', 19), (14, 5, '2000-01-05', 20),
(15, 1, '2000-01-01', 1), (15, 2, '2000-01-02', 2), (15, 3, '2000-01-03', 3), (15, 4, '2000-01-04', 4), (15, 5, '2000-01-05', 5),
(16, 1, '2000-01-06', 6), (16, 2, '2000-01-06', 7), (16, 3, '2000-01-03', 8), (16, 4, '2000-01-06', 9), (16, 5, '2000-01-06', 10),
(17, 1, '2000-01-06', 31), (17, 2, '2000-01-06', 32), (17, 3, '2000-01-03', 33), (17, 4, '2000-01-06', 34), (17, 5, '2000-01-06', 35),
(18, 1, '2000-01-06', 36), (18, 2, '2000-01-06', 37), (18, 3, '2000-01-03', 38), (18, 4, '2000-01-06', 39), (18, 5, '2000-01-06', 40),
(19, 1, '2000-01-06', 1), (19, 2, '2000-01-06', 2), (19, 3, '2000-01-03', 3), (19, 4, '2000-01-06', 4), (19, 5, '2000-01-06', 5);


INSERT INTO t1 VALUES
(21, 1, '2000-01-01', 1), (21, 2, '2000-01-02', 2), (31, 3, '2000-01-03', 3), (41, 4, '2000-01-04', 4), (51, 5, '2000-01-05', 5),
(22, 1, '2000-01-01', 6), (22, 2, '2000-01-02', 7), (32, 3, '2000-01-03', 8), (42, 4, '2000-01-04', 9), (52, 5, '2000-01-05', 10),
(23, 1, '2000-01-01', 11), (23, 2, '2000-01-02', 12), (33, 3, '2000-01-03', 13), (43, 4, '2000-01-04', 14), (53, 5, '2000-01-05', 15),
(24, 1, '2000-01-01', 16), (24, 2, '2000-01-02', 17), (34, 3, '2000-01-03', 18), (44, 4, '2000-01-04', 19), (54, 5, '2000-01-05', 20),
(25, 1, '2000-01-01', 1), (25, 2, '2000-01-02', 2), (35, 3, '2000-01-03', 3), (45, 4, '2000-01-04', 4), (55, 5, '2000-01-05', 5),
(26, 1, '2000-01-06', 6), (26, 2, '2000-01-06', 7), (36, 3, '2000-01-03', 8), (46, 4, '2000-01-06', 9), (56, 5, '2000-01-06', 10),
(27, 1, '2000-01-06', 31), (27, 2, '2000-01-06', 32), (37, 3, '2000-01-03', 33), (47, 4, '2000-01-06', 34), (57, 5, '2000-01-06', 35),
(28, 1, '2000-01-06', 36), (28, 2, '2000-01-06', 37), (38, 3, '2000-01-03', 38), (48, 4, '2000-01-06', 39), (58, 5, '2000-01-06', 40),
(29, 1, '2000-01-06', 1), (29, 2, '2000-01-06', 2), (39, 3, '2000-01-03', 3), (49, 4, '2000-01-06', 4), (59, 5, '2000-01-06', 5);

INSERT INTO t1 SELECT pk_1 + 60, pk_2, f1, f2 FROM t1;
INSERT INTO t1 SELECT pk_1 + 120, pk_2, f1, f2 FROM t1;
INSERT INTO t1 SELECT pk_1 + 240, pk_2, f1, f2 FROM t1;
INSERT INTO t1 SELECT pk_1, pk_2 + 10, f1, f2 FROM t1;

ANALYZE TABLE t1;

--echo #
--echo # REF access optimization
--echo #

--replace_column 10 #
EXPLAIN SELECT count(*) FROM t1 WHERE pk_1 = 3 and f1 = '2000-01-03';
FLUSH STATUS;
SELECT count(*) FROM t1 WHERE pk_1 = 3 and f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT pk_2 FROM t1 WHERE pk_1 = 3 and f1 = '2000-01-03';
FLUSH STATUS;
SELECT pk_2 FROM t1 WHERE pk_1 = 3 and f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--echo #
--echo # RANGE access optimization
--echo #

--replace_column 10 #
EXPLAIN SELECT count(*) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
FLUSH STATUS;
SELECT count(*) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT pk_1, pk_2 FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
FLUSH STATUS;
SELECT pk_1, pk_2 FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--echo #
--echo # MAX/MIN optimization
--echo #

--replace_column 10 #
EXPLAIN SELECT MIN(pk_1) FROM t1 WHERE f1 = '2000-01-03';
FLUSH STATUS;
SELECT MIN(pk_1) FROM t1 WHERE f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT MIN(pk_1) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
FLUSH STATUS;
SELECT MIN(pk_1) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT MAX(pk_1) FROM t1 WHERE f1 = '2000-01-03';
FLUSH STATUS;
SELECT MAX(pk_1) FROM t1 WHERE f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT MAX(pk_1) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
FLUSH STATUS;
SELECT MAX(pk_1) FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--echo #
--echo # Loose index scan
--echo #

--replace_column 10 #
EXPLAIN SELECT MIN(pk_1) FROM t1 WHERE f2 BETWEEN 13 AND 14 GROUP BY f2;
FLUSH STATUS;
SELECT MIN(pk_1) FROM t1 WHERE f2 BETWEEN 13 AND 14 GROUP BY f2;
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT MIN(pk_1) FROM t1 WHERE f2 IN (1, 2) GROUP BY f2;
FLUSH STATUS;
SELECT MIN(pk_1) FROM t1 WHERE f2 IN (1, 2) GROUP BY f2;
SHOW STATUS LIKE 'handler_read%';

--echo #
--echo # JOIN optimization
--echo #

--replace_column 10 #
EXPLAIN SELECT count(*) FROM t1 AS t1 JOIN t1 AS t2
ON t2.pk_1 = t1.pk_1 WHERE t1.f1 = '2000-01-03' AND t2.f1 = '2000-01-03';
FLUSH STATUS;
SELECT count(*) FROM t1 AS t1 JOIN t1 AS t2
ON t2.pk_1 = t1.pk_1 WHERE t1.f1 = '2000-01-03' AND t2.f1 = '2000-01-03';
SHOW STATUS LIKE 'handler_read%';

--echo #
--echo # Optimization of sorting
--echo #

--replace_column 10 #
EXPLAIN SELECT f1, pk_1, pk_2 FROM t1 WHERE pk_1 = 3 AND f1 = '2000-01-03'
ORDER BY pk_2 DESC LIMIT 5;
FLUSH STATUS;
SELECT f1, pk_1, pk_2 FROM t1 WHERE pk_1 = 3 AND f1 = '2000-01-03'
ORDER BY pk_2 DESC LIMIT 5;
SHOW STATUS LIKE 'handler_read%';

--replace_column 10 #
EXPLAIN SELECT f1, pk_1, pk_2 FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03'
ORDER BY pk_2 DESC LIMIT 5;
FLUSH STATUS;
SELECT f1, pk_1, pk_2 FROM t1 WHERE pk_1 BETWEEN 3 AND 5 AND f1 = '2000-01-03'
ORDER BY pk_2 DESC LIMIT 5;
SHOW STATUS LIKE 'handler_read%';

DROP TABLE t1;

--echo #
--echo # Max key part limitation
--echo #

CREATE TABLE t1
(
  f1 INT, f2 INT, f3 INT, f4 INT, f5 INT, f6 INT, f7 INT, f8 INT, f9 INT, f10 INT,
  f11 INT, f12 INT, f13 INT, f14 INT, f15 INT, f16 INT, f17 INT, f18 INT,
  PRIMARY KEY (f1, f2, f3, f4, f5, f6, f7, f8, f9, f10),
  KEY k1 (f11, f12, f13, f14, f15, f16, f17)
) ENGINE = InnoDB;

--replace_column 10 #
EXPLAIN SELECT f17 FROM t1 FORCE INDEX (k1) WHERE
f1 = 0 AND f2 = 0 AND f3 = 0 AND f4 = 0 AND f5 = 0 AND
f6 = 0 AND f7 = 0 AND f8 = 0 AND f9 = 0 AND f10 = 0 AND
f11 = 0 AND f12 = 0 AND f13 = 0 AND f14 = 0 AND
f15 = 0 AND f16 = 0 AND f17 = 0;

--replace_column 10 #
EXPLAIN SELECT f17 FROM t1 FORCE INDEX (k1) WHERE
f1 = 0 AND f2 = 0 AND f3 = 0 AND f4 = 0 AND f5 = 0 AND
f6 = 0 AND f7 = 0 AND f8 = 0 AND f9 = 0 AND
f11 = 0 AND f12 = 0 AND f13 = 0 AND f14 = 0 AND
f15 = 0 AND f16 = 0 AND f17 = 0;

DROP TABLE t1;

--echo #
--echo # Max key length limitation
--echo #

CREATE TABLE t1
(
  f1 VARCHAR(500), f2 VARCHAR(500), f3 VARCHAR(500),
  f4 VARCHAR(500), f5 VARCHAR(500), f6 VARCHAR(500),
  f7 VARCHAR(500),
  PRIMARY KEY (f1, f2, f3, f4),
  KEY k1 (f5, f6, f7)
) ENGINE = InnoDB;

--replace_column 10 #
EXPLAIN SELECT f5 FROM t1 FORCE INDEX (k1) WHERE
f1 = 'a' AND f2 = 'a' AND f3 = 'a' AND f4 = 'a' AND
f5 = 'a' AND f6 = 'a' AND f7 = 'a';

--replace_column 10 #
EXPLAIN SELECT f5 FROM t1 FORCE INDEX (k1) WHERE
f1 = 'a' AND f2 = 'a' AND f3 = 'a' AND
f5 = 'a' AND f6 = 'a' AND f7 = 'a';

--replace_column 10 #
EXPLAIN SELECT f5 FROM t1 FORCE INDEX (k1) WHERE
f1 = 'a' AND f2 = 'a' AND f4 = 'a' AND
f5 = 'a' AND f6 = 'a' AND f7 = 'a';

DROP TABLE t1;

--echo #
--echo # Unique extended key
--echo #

CREATE TABLE t1
(
  pk INT NOT NULL auto_increment,
  f1 INT NOT NULL,
  KEY (f1),
  PRIMARY KEY (pk)
) ENGINE = INNODB;

CREATE TABLE t2
(
  f1 INT,
  f2 INT
) ENGINE = INNODB;

INSERT INTO t1(f1) VALUES (1),(2);
INSERT INTO t1(f1) SELECT f1 + 2 FROM t1;
INSERT INTO t1(f1) SELECT f1 + 4 FROM t1;
ANALYZE TABLE t1;

INSERT INTO t2 VALUES (1,1), (2,2);

EXPLAIN SELECT t2.f1 FROM t2 JOIN t1 IGNORE INDEX(primary) ON t2.f1 = t1.pk and t2.f2 = t1.f1;
FLUSH STATUS;
SELECT t2.f1 FROM t2 JOIN t1 IGNORE INDEX(primary) ON t2.f1 = t1.pk and t2.f2 = t1.f1;
SHOW STATUS LIKE 'Handler_read%';

DROP TABLE t1, t2;

--echo #
--echo # Bug#20430526 ADDRESSSANITIZER: HEAP-BUFFER-OVERFLOW IN
--echo #              FIELD_BLOB::GET_KEY_IMAGE FLOAT8STORE
--echo #

CREATE TABLE t1
(a GEOMETRY NOT NULL, PRIMARY KEY (a(1)), SPATIAL KEY(a)) ENGINE=INNODB;

INSERT INTO t1 VALUES (point(7, 7));
SELECT st_astext(a) FROM t1 WHERE st_equals(a, point(7, 7));
DROP TABLE t1;
