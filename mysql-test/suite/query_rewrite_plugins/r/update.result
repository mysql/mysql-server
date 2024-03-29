#
# Test of rewrites of update statements.
#
use test;
CREATE TABLE t1 (a INT NOT NULL, b int, PRIMARY KEY (a));
CREATE TABLE t2 (a int);
INSERT INTO t1 VALUES (1, 10), (2, 20), (3, 30);
INSERT INTO t2 SELECT a FROM t1;
# Query rewrite plugin was installed.
INSERT INTO query_rewrite.rewrite_rules ( pattern, pattern_database, replacement )
VALUES ( 'UPDATE test.t1 SET a = ? WHERE b = ? ORDER BY ? LIMIT ?', 'test',
'UPDATE test.t1 SET b = ? WHERE b = ? ORDER BY a LIMIT 4'),
( 'UPDATE test.t1, test.t2 SET t1.a = ?, t2.a = ? WHERE t1.b = ?', 'test',
'UPDATE test.t1, test.t2 SET t1.a = ?, t1.b = ? WHERE t1.a = ?' ),
('UPDATE t1 LEFT JOIN t2 ON t1.a = ? SET t1.b = 20 WHERE t1.b > 25', 'test',
'UPDATE t1 LEFT JOIN t2 ON t2.a = ? SET t1.b = 20 WHERE t1.b > 25') ;
CALL query_rewrite.flush_rewrite_rules();
SELECT * FROM query_rewrite.rewrite_rules;
id	pattern	pattern_database	replacement	enabled	message
1	UPDATE test.t1 SET a = ? WHERE b = ? ORDER BY ? LIMIT ?	test	UPDATE test.t1 SET b = ? WHERE b = ? ORDER BY a LIMIT 4	YES	NULL
2	UPDATE test.t1, test.t2 SET t1.a = ?, t2.a = ? WHERE t1.b = ?	test	UPDATE test.t1, test.t2 SET t1.a = ?, t1.b = ? WHERE t1.a = ?	YES	NULL
3	UPDATE t1 LEFT JOIN t2 ON t1.a = ? SET t1.b = 20 WHERE t1.b > 25	test	UPDATE t1 LEFT JOIN t2 ON t2.a = ? SET t1.b = 20 WHERE t1.b > 25	YES	NULL
UPDATE test.t1 SET a = 0 WHERE b = 2 ORDER BY 3 LIMIT 2;
Warnings:
Note	1105	Query 'UPDATE test.t1 SET a = 0 WHERE b = 2 ORDER BY 3 LIMIT 2' rewritten to 'UPDATE test.t1 SET b = 0 WHERE b = 2 ORDER BY a LIMIT 4' by a query rewrite plugin
UPDATE test.t1 SET a = 0 WHERE b = 2 ORDER BY a LIMIT 2;
SELECT * FROM t1;
a	b
1	10
2	20
3	30
UPDATE test.t1, test.t2 SET t1.a = 2, t2.a = 25 WHERE t1.b = 21;
Warnings:
Note	1105	Query 'UPDATE test.t1, test.t2 SET t1.a = 2, t2.a = 25 WHERE t1.b = 21' rewritten to 'UPDATE test.t1, test.t2 SET t1.a = 2, t1.b = 25 WHERE t1.a = 21' by a query rewrite plugin
SELECT * FROM t1;
a	b
1	10
2	20
3	30
SELECT * FROM t2;
a
1
2
3
UPDATE t1 LEFT JOIN t2 ON t1.a = 25 SET t1.b = 20 WHERE t1.b > 25;
Warnings:
Note	1105	Query 'UPDATE t1 LEFT JOIN t2 ON t1.a = 25 SET t1.b = 20 WHERE t1.b > 25' rewritten to 'UPDATE t1 LEFT JOIN t2 ON t2.a = 25 SET t1.b = 20 WHERE t1.b > 25' by a query rewrite plugin
DROP TABLE t1, t2;
Warnings:
Warning	1620	Plugin is busy and will be uninstalled on shutdown
# Query rewrite plugin was queued for uninstalling.
