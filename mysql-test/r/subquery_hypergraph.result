#
# Bug#33280189: Hypergraph: Some subqueries are evaluated
#               during optimization
#
CREATE TABLE t(x INTEGER);
INSERT INTO t VALUES (1), (2);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
EXPLAIN FORMAT=TREE SELECT * FROM t WHERE (SELECT x FROM t);
EXPLAIN
-> Filter: <cache>((0 <> (select #2)))
    -> Table scan on t
    -> Select #2 (subquery in condition; run only once)
        -> Table scan on t

SELECT * FROM t WHERE (SELECT x FROM t);
ERROR 21000: Subquery returns more than 1 row
DROP TABLE t;
