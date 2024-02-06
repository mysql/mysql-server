--source include/skip_scan_data.inc
--source include/elide_costs.inc

# These queries should do loose index scans.

# Skip scan is used if 'skip_scan=on'
--let $query= SELECT b, d FROM t WHERE d < 2
--let $hint_query= SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d < 2
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT b, d FROM t WHERE d > 4
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, d FROM t WHERE d > 4
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a = 5 AND d <= 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d <= 3
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a = 5 AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5) AND b IN (1, 2) AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 2, 3, 4, 5) AND b IN (1, 2, 3) AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = 5 AND b = 2 AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT a+1, b, c, d FROM t WHERE a = 5 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a+1, b, c, d FROM t WHERE a = 5 AND d < 3
--source include/skip_scan_query.inc

# Skip scan is not used if 'skip_scan=on' due to cost
--let $query = SELECT b, c, d FROM t WHERE a = 5 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND d < 3
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT a, b, c, d FROM t WHERE a = b AND d >= 98
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a = b AND d >= 98
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c
--source include/skip_scan_query.inc

# Skip scan is used if 'skip_scan=on'
--let $query = SELECT b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 1 AND d < 3 ORDER BY d, c * -1
--source include/skip_scan_query.inc

# Skip scan is not used since field 'e' is not part of the index.
--let $query = SELECT e FROM t WHERE a = 5 AND d <= 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ e FROM t WHERE a = 5 AND d <= 3
--source include/skip_scan_query.inc

# Skip scan is not used since field 'e' is not part of the index.
--let $query = SELECT b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 10 AND d < 3 ORDER BY d, c, e
--source include/skip_scan_query.inc

# Testing aggregate functions.
--let $query = SELECT count(a), count(b), count(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ count(a), count(b), count(c) FROM t WHERE a = 5 AND c > 10 AND d < 3
--source include/skip_scan_query.inc

--let $query = SELECT min(a), max(b), min(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ min(a), max(b), min(c) FROM t WHERE a = 5 AND c > 10 AND d < 3
--source include/skip_scan_query.inc

--let $query = SELECT sum(a), sum(b), sum(c)  FROM t WHERE a = 5 AND c > 10 AND d < 3
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ sum(a), avg(b), sum(c) FROM t WHERE a = 5 AND c > 10 AND d < 3
--source include/skip_scan_query.inc

--let $query = SELECT count(*) FROM t WHERE d < 2;
--let $hint_query = SELECT /*+ SKIP_SCAN(t) */ count(*) FROM t WHERE d < 2;
--source include/skip_scan_query.inc

if ($engine == 'innodb' )
{
  --echo Testing DESC index with skip scan.
  ALTER TABLE t DROP PRIMARY KEY;
  ALTER TABLE t DROP KEY b;
  ALTER TABLE t ADD PRIMARY KEY(a DESC, b, c DESC, d);
  ANALYZE TABLE t;

  --let $query = SELECT b, c, d FROM t WHERE a = 5 AND c > 3 AND d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c > 3 AND d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT b, c, d FROM t WHERE a = 5 AND c < 3 AND d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c < 3 AND d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT b, c, d FROM t WHERE a = 5 AND c = 3 AND d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c = 3 AND d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT b, c, d FROM t WHERE a = 5 AND c BETWEEN 3 AND 5 AND d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE a = 5 AND c BETWEEN 3 AND 5 AND d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT b, c, d FROM t WHERE c BETWEEN 3 AND 5 AND d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE c BETWEEN 3 AND 5 AND d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT b, c, d FROM t WHERE d < 3;
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ b, c, d FROM t WHERE d < 3;
  --source include/skip_scan_query.inc

  --let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 5
  --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 3, 5) AND d >= 5
  --source include/skip_scan_query.inc

 --let $query = SELECT a, b, c, d FROM t WHERE a IN (1, 5, 3) AND b IN (1, 2) AND d >= 5
 --let $hint_query = SELECT /*+ SKIP_SCAN(t) */ a, b, c, d FROM t WHERE a IN (1, 5, 3) AND b in (1, 2) AND d >= 5
 --source include/skip_scan_query.inc
}

set optimizer_switch = default;
DROP TABLE t;

if ($engine == 'innodb' )
{
  --echo Testing DESC index with skip scan and NULL range.
  CREATE TABLE t1 (a INT, b INT, c INT, d INT, e INT,
                   KEY k1(a DESC, b, c DESC, d, e DESC));

  INSERT INTO t1 VALUES
  (NULL, 1, 1, 3, 4), (NULL, 2, 1, 4, 5),
  (1, 2, 1, 3, 4), (2, 2, 1, 4, 5),
  (5, 2, 3, 3, 4), (2, 2, 1, 4, 11),
  (8, 2, 1, 3, 4), (7, 2, 1, 4, 9);

  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;

  ANALYZE TABLE t1;

  --let $query = SELECT a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --let $hint_query = SELECT /*+ SKIP_SCAN(t1) */ a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --source include/skip_scan_query.inc

  DROP TABLE t1;

  CREATE TABLE t1 (a INT, b INT, c INT, d INT, e INT,
                   KEY k1(a, b DESC, c, d DESC, e));

  INSERT INTO t1 VALUES
  (NULL, 1, 1, 3, 4), (NULL, 2, 1, 4, 5),
  (1, 2, 1, 3, 4), (2, 2, 1, 4, 5),
  (5, 2, 3, 3, 4), (2, 2, 1, 4, 11),
  (8, 2, 1, 3, 4), (7, 2, 1, 4, 9);

  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;

  ANALYZE TABLE t1;

  --let $query = SELECT a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --let $hint_query = SELECT /*+ SKIP_SCAN(t1) */ a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --source include/skip_scan_query.inc

  set optimizer_switch = default;
  DROP TABLE t1;
}

--echo #
--echo # Bug#28089360 WL#11322: SIG11 AT QEP_SHARED_OWNER::JOIN | SQL/SQL_OPT_EXEC_SHARED.H:458
--echo #

CREATE TABLE t1 (f1 INT(11), f2 VARCHAR(1), KEY k1 (f2, f1));
INSERT INTO t1 VALUES (-682212662,'c'), (-1974921822,'C'), (1260604936,'9');

CREATE TABLE t2 (f1 INT(11));
INSERT INTO t2 VALUES (824388284), (1186821161);

CREATE VIEW v1 AS select f1, f2 from t1;

DELETE FROM t2 WHERE (f1, f1) IN (SELECT f1,f2 FROM v1 WHERE f1 >= 2);

DROP VIEW v1;
DROP TABLE t1, t2;

if ($engine == innodb)
{
  --echo #
  --echo # Bug #29602393 ASSERTION `BITMAP_IS_SET(KEY_INFO->TABLE->READ_SET,
  --echo #                                        KEY_INFO->KEY_PART.*FAILED
  --echo #

  CREATE TABLE t1(f1 INT, f2 VARCHAR(64) NOT NULL, PRIMARY KEY (f1))
  PARTITION BY RANGE(f1)
  (
    PARTITION p1 VALUES LESS THAN (3),
    PARTITION p2 VALUES LESS THAN (maxvalue)
  );
  INSERT INTO t1 VALUES (1, 'abcde'), (2, 'abcde'), (3, 'abcde');
  CREATE INDEX idx1 ON t1(f2);
  ANALYZE TABLE t1;

  --replace_regex $elide_costs
  EXPLAIN FORMAT=tree SELECT /*+ SKIP_SCAN(t1 idx1) */ count(*) FROM t1 WHERE f1 <= 3;
  SELECT /*+ SKIP_SCAN(t1 idx1) */ count(*) FROM t1 WHERE f1 <= 3;

  DROP TABLE t1;
}

--echo #
--echo # Bug 33251616 - NO_SKIP_SCAN hint disables skip scan for all indexes
--echo #

CREATE TABLE t1(c1 INT, c2 INT, c3 INT, c4 INT);
CREATE UNIQUE INDEX i1_t1 ON t1(c1,c2,c3);
CREATE UNIQUE INDEX i2_t1 ON t1(c1,c2,c4);
INSERT INTO t1 VALUES (1,1,1,1), (1,1,2,2), (1,3,3,3), (1,4,4,4), (1,5,5,5),
                      (2,1,1,1), (2,2,2,2), (2,3,3,3), (2,4,4,4), (2,5,5,5);
INSERT INTO t1 SELECT c1, c2, c3+5, c4+10  FROM t1;
INSERT INTO t1 SELECT c1, c2, c3+10, c4+20 FROM t1;
INSERT INTO t1 SELECT c1, c2, c3+20, c4+40 FROM t1;
INSERT INTO t1 SELECT c1, c2, c3+40, c4+80 FROM t1;
ANALYZE TABLE t1;

--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT c1, c2 FROM t1 WHERE c2 > 40;

# the other index(i2_t1) should be used for skip_scan
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT /*+NO_SKIP_SCAN(t1 i1_t1)*/ c1, c2 FROM t1 WHERE c2 > 40;

# index i1_t1 should be used for skip_scan
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT /*+NO_SKIP_SCAN(t1 i2_t1)*/ c1, c2 FROM t1 WHERE c2 > 40;

# skip_scan should not be used for table t1
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT /*+NO_SKIP_SCAN(t1)*/  c1, c2 FROM t1 WHERE c2 > 40;

# force skip_scan to use index i2_t1
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT /*+SKIP_SCAN(t1 i2_t1)*/  c1, c2 FROM t1 WHERE c2 > 40;

SET OPTIMIZER_SWITCH = 'skip_scan=off';
# skip_scan should not be used
--replace_regex $elide_costs
EXPLAIN FORMAT=tree SELECT c1, c2 FROM t1 WHERE c2 > 40;

DROP TABLE t1;
SET OPTIMIZER_SWITCH = default;
