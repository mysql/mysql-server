CREATE TABLE t1(i INT, j INT);
INSERT INTO t1 VALUES (0,0),(1,1),(2,2),(3,3),(4,4);
ANALYZE TABLE t1;

--echo #
--echo # Bug#34302461: EXPLAIN cost and row estimates are wrong for UNION
--echo #

--echo # These two queries get the same plan and should therefore have the
--echo # same row and cost estimates.
EXPLAIN FORMAT=TREE SELECT i FROM t1 UNION SELECT 7 ORDER BY i;
EXPLAIN FORMAT=TREE SELECT * FROM (SELECT i FROM t1 UNION SELECT 7) d1
  ORDER BY i;
  
--echo # These two queries get the same plan and should therefore have the
--echo # same row and cost estimates.
EXPLAIN FORMAT=TREE SELECT * FROM t1 UNION SELECT * FROM t1 ORDER BY i;
EXPLAIN FORMAT=TREE SELECT * FROM (SELECT * FROM t1 UNION SELECT * FROM t1)
  d1 ORDER BY i;


--echo #
--echo # Bug#33834146: 'EXPLAIN ANALYZE' cost + elapsed time not cumulative
--echo # when using materialization
--echo #
EXPLAIN FORMAT=TREE INSERT INTO t1 SELECT MAX(i), 3 FROM
  (SELECT * FROM t1 ORDER BY j LIMIT 5) AS d1;

DROP TABLE t1;

