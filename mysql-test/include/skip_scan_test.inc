--source include/skip_scan_data.inc

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
  CREATE TABLE t1 (a INT, b INT, c INT, d INT, e INT);

  INSERT INTO t1 VALUES
  (NULL, 1, 1, 3, 4), (NULL, 2, 1, 4, 5),
  (1, 2, 1, 3, 4), (2, 2, 1, 4, 5),
  (5, 2, 3, 3, 4), (2, 2, 1, 4, 11),
  (8, 2, 1, 3, 4), (7, 2, 1, 4, 9);

  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;
  INSERT INTO t1 SELECT * FROM t1;

  ALTER TABLE t1 ADD KEY k1(a DESC, b, c DESC, d, e DESC);
  ANALYZE TABLE t1;

  --let $query = SELECT a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --let $hint_query = SELECT /*+ SKIP_SCAN(t1) */ a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --source include/skip_scan_query.inc

  ALTER TABLE t1 DROP KEY k1;
  ALTER TABLE t1 ADD KEY k1(a, b DESC, c, d DESC, e);
  ANALYZE TABLE t1;

  --let $query = SELECT a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --let $hint_query = SELECT /*+ SKIP_SCAN(t1) */ a, b, c, d FROM t1 WHERE (a IN (1, 5) or a IS NULL) AND b in (1, 2) AND d BETWEEN 2 AND 5
  --source include/skip_scan_query.inc

  set optimizer_switch = default;
  DROP TABLE t1;
}
