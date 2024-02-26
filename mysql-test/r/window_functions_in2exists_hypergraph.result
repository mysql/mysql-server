CREATE TABLE t(a INT);
INSERT INTO t VALUES (1),(2),(3);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
=ALL => NOT EXISTS (select non-equal rows)
SELECT * FROM t WHERE a = ALL(SELECT ROW_NUMBER() OVER () + 1 FROM t);
a
<>SOME => EXISTS (select non-equal rows)
SELECT * FROM t WHERE a <> SOME(SELECT ROW_NUMBER() OVER () + 1 FROM t);
a
1
2
3
SELECT * FROM t WHERE a <> ANY(SELECT ROW_NUMBER() OVER () + 1 FROM t);
a
1
2
3
# Correlated => no subquery materialization.
# IN => EXISTS, error
SELECT * FROM t upper
WHERE upper.a IN (SELECT ROW_NUMBER() OVER () FROM t
WHERE t.a > upper.a);
a
1
# Correlated => no subquery materialization.
# >ANY => EXISTS(select smaller rows), error
SELECT * FROM t upper
WHERE upper.a > ANY (SELECT ROW_NUMBER() OVER () FROM t
WHERE t.a > upper.a);
a
2
SELECT * FROM t upper
WHERE upper.a > ALL (SELECT ROW_NUMBER() OVER () FROM t
WHERE t.a > upper.a);
a
2
3
DROP TABLE t;
#
# Bug#25601138 WINDOW FUNCTION IN CORRELATED SUBQUERY GIVES WRONG RESULT
#
CREATE TABLE t1(i INT, j INT, k INT);
INSERT INTO t1 VALUES (1,1,1),(2,2,2);
select * from t1 AS upper where i+1
IN (select row_number() over () + upper.i from t1 );
i	j	k
1	1	1
2	2	2
DROP TABLE t1;
