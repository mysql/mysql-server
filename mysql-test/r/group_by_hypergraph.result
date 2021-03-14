#
# Bug#32980875: WL#14422: ASSERTION `FALSE' FAILED|SQL/BASIC_ROW_ITERATORS.H
#
CREATE TABLE t (x INTEGER);
INSERT INTO t VALUES (1), (2), (3);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
EXPLAIN FORMAT=TREE
SELECT DISTINCT MIN(t2.x), t1.x
FROM t t1 JOIN t t2 USING (x)
GROUP BY t2.x;
EXPLAIN
-> Remove duplicates from input grouped on t.x, `MIN(t2.x)`
    -> Stream results
        -> Group aggregate: min(t2.x)
            -> Sort: t2.x
                -> Inner hash join (t1.x = t2.x)
                    -> Table scan on t1
                    -> Hash
                        -> Table scan on t2

DROP TABLE t;
