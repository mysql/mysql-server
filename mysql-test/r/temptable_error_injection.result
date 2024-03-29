CREATE TABLE t (c INT);
INSERT INTO t VALUES (1);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
Pattern "Aggregate using temporary table" found
SET debug = '+d,temptable_create_return_full';
SELECT * FROM (SELECT COUNT(*) FROM t GROUP BY c) as dt;
COUNT(*)
1
SET debug = '-d,temptable_create_return_full';
DROP TABLE t;
