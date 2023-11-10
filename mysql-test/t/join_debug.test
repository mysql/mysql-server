--source include/have_debug.inc

--echo #
--echo # Bug#35208539 - Server abort from test_if_ref sql/sql_optimizer.cc
--echo #

CREATE TABLE t1(f1 INTEGER, f2 INTEGER, KEY(f1), KEY(f2));
CREATE TABLE t2 LIKE t1;
CREATE TABLE t3 LIKE t1;

SET debug= '+d,bug35208539_raise_error';
--error ER_GET_ERRNO
SELECT * FROM t1 WHERE t1.f1 < 1;
SET debug= '-d,bug35208539_raise_error';
SELECT MIN(t3.f1)
FROM (t2 JOIN (t3 JOIN (SELECT t1.*
                        FROM t1
                        WHERE t1.f2 < t1.f2) AS dt
               ON (dt.f1 = t3.f1))
      ON (dt.f2 = t3.f2))
WHERE (dt.f2 <> ANY (SELECT t1.f1 FROM t1 WHERE t1.f2 = dt.f2));

DROP TABLE t1, t2, t3;

--echo #
--echo # Bug#35991384: Mysqld crash from Query_block::optimize()
--echo #               Assertion `join == nullptr'
--echo #

CREATE TABLE t(x INT);
INSERT INTO t VALUES (1), (2), (3), (4), (5), (6);
ANALYZE TABLE t;
SET debug = '+d,kill_query_in_hash_join_iterator';
--error ER_QUERY_INTERRUPTED
SELECT COUNT(*) WHERE (
  SELECT 1 FROM (SELECT COUNT(*) FROM t AS t1, t AS t2 WHERE t1.x=t2.x) AS dt
);
SET debug = '-d,kill_query_in_hash_join_iterator';
DROP TABLE t;
