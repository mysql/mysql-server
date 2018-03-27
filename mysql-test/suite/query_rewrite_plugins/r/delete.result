#
# Test of rewrites of delete statements.
#
use test;
CREATE TABLE t1 (
a INT,
b INT )
PARTITION BY LIST(a) (
PARTITION p0 VALUES IN (1, 3),
PARTITION p1 VALUES IN (5, 7)
);
CREATE TABLE t2 ( a INT, b INT );
INSERT INTO t1 VALUES (1, 2), (3, 4), (5, 2), (7, 4);
INSERT INTO t2 VALUES (10, 20), (30, 40), (50, 60);
CREATE VIEW v1 AS SELECT t1.* FROM t1 JOIN t2 ON t1.b=t2.b;
CREATE VIEW v2 AS SELECT b, a FROM t1 WHERE b > 20;
# Query rewrite plugin was installed.
INSERT INTO query_rewrite.rewrite_rules ( pattern, replacement )
VALUES ( 'DELETE FROM test.t1 PARTITION (p0) WHERE a = ? ORDER BY b LIMIT 2',
'DELETE FROM test.t1 PARTITION (p0) WHERE b = ? ORDER BY b LIMIT 2' ),
( 'DELETE test.t1 FROM test.t1 join test.t2 WHERE test.t1.a = ?',
'DELETE test.t1 FROM test.t1 join test.t2 WHERE test.t1.a IN (?,3,5)' ),
( 'DELETE test.v1 FROM test.v1 join test.t2 WHERE test.v1.a = ?',
'DELETE test.v1 FROM test.v1 join test.t2 WHERE test.v1.a IN (?,3,5)' ),
( 'DELETE test.v2 FROM test.v2 WHERE test.v2.a = ?',
'DELETE test.v2 FROM test.v2 WHERE test.v2.a IN (?,3,5)' ),
( 'DELETE FROM test.v1',
'DELETE FROM test.t2' );
CALL query_rewrite.flush_rewrite_rules();
DELETE FROM test.t1 PARTITION (p0) WHERE a = 4 ORDER BY b LIMIT 2;
Warnings:
Note	1105	Query 'DELETE FROM test.t1 PARTITION (p0) WHERE a = 4 ORDER BY b LIMIT 2' rewritten to 'DELETE FROM test.t1 PARTITION (p0) WHERE b = 4 ORDER BY b LIMIT 2' by a query rewrite plugin
SELECT * FROM t1;
a	b
1	2
5	2
7	4
DELETE test.t1 FROM test.t1 join test.t2 WHERE test.t1.a = 1;
Warnings:
Note	1105	Query 'DELETE test.t1 FROM test.t1 join test.t2 WHERE test.t1.a = 1' rewritten to 'DELETE test.t1 FROM test.t1 join test.t2 WHERE test.t1.a IN (1,3,5)' by a query rewrite plugin
SELECT * FROM t1;
a	b
7	4
SELECT * FROM t2;
a	b
10	20
30	40
50	60
DELETE test.v1 FROM test.v1 join test.t2 WHERE test.v1.a = 1;
ERROR HY000: Can not delete from join view 'test.v1'
DELETE test.v2 FROM test.v2 WHERE test.v2.a = 1;
Warnings:
Note	1105	Query 'DELETE test.v2 FROM test.v2 WHERE test.v2.a = 1' rewritten to 'DELETE test.v2 FROM test.v2 WHERE test.v2.a IN (1,3,5)' by a query rewrite plugin
DELETE FROM test.v1;
Warnings:
Note	1105	Query 'DELETE FROM test.v1' rewritten to 'DELETE FROM test.t2' by a query rewrite plugin
DROP VIEW v1, v2;
DROP TABLE t1, t2;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
# Query rewrite plugin was queued for uninstalling.
