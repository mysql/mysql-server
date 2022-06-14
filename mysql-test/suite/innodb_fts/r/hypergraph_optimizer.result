CREATE TABLE t(x VARCHAR(100), FULLTEXT KEY (x));
INSERT INTO t VALUES ('abc'), ('xyz'), (NULL), ('abc abc'), ('abc xyz');
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SELECT a.x, MATCH (a.x) AGAINST ('abc') AS score_a,
b.x, MATCH (b.x) AGAINST ('xyz') AS score_b
FROM t a, t b WHERE a.x = b.x;
x	score_a	x	score_b
abc	0.0492168664932251	abc	0
abc abc	0.0984337329864502	abc abc	0
abc xyz	0.0492168664932251	abc xyz	0.15835624933242798
xyz	0	xyz	0.15835624933242798
SELECT x, MATCH (x) AGAINST ('abc') AS score FROM t
WHERE MATCH (x) AGAINST ('abc') > NULL;
x	score
SELECT x, MATCH (x) AGAINST ('abc') AS score FROM t
WHERE MATCH (x) AGAINST ('abc')
AND MATCH (x) AGAINST ('abc') > 0.05;
x	score
abc abc	0.0984337329864502
SELECT x, MATCH (x) AGAINST ('abc') AS score FROM t
WHERE MATCH (x) AGAINST ('abc') > 0.05
AND MATCH (x) AGAINST ('abc');
x	score
abc abc	0.0984337329864502
SELECT x, MATCH (x) AGAINST ('abc') AS score FROM t GROUP BY x;
x	score
NULL	0
abc	0.0492168664932251
abc abc	0.0984337329864502
abc xyz	0.0492168664932251
xyz	0
SELECT x, MATCH (x) AGAINST ('abc') AS score FROM t GROUP BY x, score;
x	score
NULL	0
abc	0.0492168664932251
abc abc	0.0984337329864502
abc xyz	0.0492168664932251
xyz	0
SELECT MATCH (x) AGAINST ('abc') AS score FROM t GROUP BY score;
score
0
0.0492168664932251
0.0984337329864502
SELECT x, MATCH (x) AGAINST ('abc')+1 AS score FROM t GROUP BY x;
x	score
NULL	1
abc	1.049216866493225
abc abc	1.0984337329864502
abc xyz	1.049216866493225
xyz	1
SELECT x, CONCAT(MATCH (x) AGAINST ('abc'), MAX(x)) AS score FROM t GROUP BY x;
x	score
NULL	NULL
abc	0.0492168664932251abc
abc abc	0.0984337329864502abc abc
abc xyz	0.0492168664932251abc xyz
xyz	0xyz
SELECT x FROM t GROUP BY x HAVING MATCH (x) AGAINST ('abc');
x
abc
abc abc
abc xyz
SELECT x FROM t GROUP BY x HAVING MATCH (x) AGAINST ('abc') > 0;
x
abc
abc abc
abc xyz
SELECT MATCH (x) AGAINST ('abc') AS score FROM t
GROUP BY score WITH ROLLUP HAVING score > 0;
score
0.0492168664932251
0.0984337329864502
SELECT MATCH (x) AGAINST ('abc') AS score FROM t
GROUP BY score WITH ROLLUP HAVING score > 0 OR score IS NULL;
score
0.0492168664932251
0.0984337329864502
NULL
SELECT MATCH (x) AGAINST ('abc') AS score, x FROM t
GROUP BY x WITH ROLLUP ORDER BY score, x;
ERROR HY000: MATCH does not support ROLLUP columns.
SELECT ANY_VALUE(x), MATCH (x) AGAINST ('abc') AS score
FROM t
WHERE x IN ('abc', 'abc abc', 'xyz')
GROUP BY score
ORDER BY score;
ANY_VALUE(x)	score
xyz	0
abc	0.0492168664932251
abc abc	0.0984337329864502
DROP TABLE t;
