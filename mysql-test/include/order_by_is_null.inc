--echo #
--echo # Bug#38700622: Optimizer doesn't simplify ORDER BY when
--echo #               field IS NULL
--echo #

CREATE TABLE t1 (
  f1 INT PRIMARY KEY AUTO_INCREMENT,
  f2 INT NOT NULL,
  f3 DATETIME DEFAULT NULL,
  f4 VARCHAR(50),
  f5 VARCHAR(20),
  f6 TINYINT,
  f7 VARCHAR(100),
  KEY k1 (f2, f3, f4, f5, f6, f7)
) ENGINE=InnoDB;

INSERT INTO t1 (f2, f3, f4, f5, f6, f7) VALUES
(100, NULL, 'User1', 'Active', 0, 'Item A'),
(100, NULL, 'User1', 'Active', 0, 'Item B'),
(100, NULL, 'User1', 'Active', 0, 'Item C'),
(100, NULL, 'User1', 'Pending', 0, 'Item D'),
(100, '2024-01-01 10:00:00', 'User1', 'Active', 0, 'Item E'),
(200, NULL, 'User2', 'Active', 1, 'Item F'),
(200, NULL, 'User3', 'Pending', 0, 'Item G');

ANALYZE TABLE t1;

--echo #
--echo # Test 1: Basic IS NULL with ORDER BY over f2, f3, f7.
--echo #         Should avoid sorting on f3.
--echo #

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f2 = 100 AND f3 IS NULL
ORDER BY f2, f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 2: Compound index with IS NULL - No external sort.
--echo #

let $query =
SELECT f1, f2, f3, f4, f5, f6, f7
FROM t1
WHERE f2 = 100 AND f3 IS NULL AND f4 = 'User1'
      AND f5 = 'Active' AND f6 = 0 AND f7 >= 'Item B'
ORDER BY f2, f3, f4, f5, f6, f7 ASC
LIMIT 10;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 3: Mix of equality and IS NULL with all intermediate
--echo #         fields constant. No external sort.
--echo #

let $query =
SELECT f1, f2, f3, f4, f5, f6, f7
FROM t1
WHERE f2 = 100 AND f3 IS NULL AND f4 = 'User1'
      AND f5 = 'Active' AND f6 = 0
ORDER BY f2, f3, f4, f5, f6, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 4: IS NOT NULL should NOT be treated as constant.
--echo #         Sorting needed.
--echo #

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f3 IS NOT NULL ORDER BY f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 5: Record optimizer trace marker for constant
--echo #         ORDER BY fields. For hypergraph it would
--echo #         be absent.
--echo #
SET optimizer_trace="enabled=on";

SELECT f1, f2, f3, f7 FROM t1
WHERE f3 IS NULL
ORDER BY f2, f3, f7 LIMIT 2;

SELECT IF(
  LOCATE('"equals_constant_in_where": true', TRACE) > 0,
  'TRACE: equals_constant_in_where present',
  'TRACE: equals_constant_in_where absent'
) AS trace_check
FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE;

SET optimizer_trace="enabled=off";

--echo #
--echo # Test 6: OR with IS NULL - field should NOT be treated
--echo #         as constant. So sorting is needed.
--echo #

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f2 = 100 AND (f3 IS NULL OR f3 > '2024-01-01')
ORDER BY f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 7: OR with multiple IS NULL - same field.
--echo #         Will be treated as constant. Sorting will be
--echo #         only on "f7".
--echo #

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f2 = 100 AND (f3 IS NULL OR f3 IS NULL)
ORDER BY f2, f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 8: OR with <=> NULL and IS NULL on the same field.
--echo #         Tradiitonal optimizer treats f3 as constant;
--echo #         hypergraph keeps f3 in the sort (Bug#39567745).
--echo #

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f2 = 100 AND (f3 <=> NULL OR f3 IS NULL)
ORDER BY f2, f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

let $query =
SELECT f1, f2, f3, f7 FROM t1
WHERE f2 = 100 AND (f3 IS NULL OR f3 <=> NULL)
ORDER BY f2, f3, f7;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 9: Verify IS NULL works with DESC ordering
--echo #

let $query =
SELECT f1, f2, f3, f4, f5, f6, f7
FROM t1
WHERE f2 = 100 AND f3 IS NULL AND f4 = 'User1'
      AND f5 = 'Active' AND f6 = 0
ORDER BY f2, f3, f4, f5, f6, f7 DESC;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 10: Verify NULL ordering semantics with ASC
--echo #

CREATE TABLE t2 (
  f1 INT PRIMARY KEY AUTO_INCREMENT,
  f2 INT,
  f3 VARCHAR(10),
  KEY k1 (f2, f3)
);
INSERT INTO t2 (f2, f3) VALUES
  (NULL, 'null1'),
  (NULL, 'null2'),
  (5, 'five'),
  (10, 'ten'),
  (NULL, 'null3'),
  (1, 'one');
ANALYZE TABLE t2;

SELECT f1, f2, f3 FROM t2 ORDER BY f2 ASC, f3 ASC;

--echo #
--echo # Test 11: Verify NULL ordering semantics with DESC
--echo #

SELECT f1, f2, f3 FROM t2 ORDER BY f2 DESC, f3 ASC;

--echo #
--echo # Test 12: Verify optimization works with DESC and preserves NULL ordering
--echo #

let $query=
SELECT f1, f2, f3 FROM t2
WHERE f2 IS NULL ORDER BY f2 DESC, f3 ASC;
eval EXPLAIN FORMAT=tree $query;
eval $query;

--echo #
--echo # Test 13: Mixed ASC/DESC with IS NULL constraint
--echo #

let $query=
SELECT f1, f2, f3 FROM t2
WHERE f2 IS NULL ORDER BY f2 ASC, f3 DESC;
eval EXPLAIN FORMAT=tree $query;
eval $query;

DROP TABLE t2;
DROP TABLE t1;
