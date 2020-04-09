#
# Bug #30769515 INCORRECT ERROR HANDLING IN GROUP-BY PLANS.
#
CREATE TABLE t (a INT, b INT, c INT, d INT, KEY k1(a, b, c, d)) ENGINE=innodb;
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET debug= '+d,bug30769515_QUERY_INTERRUPTED';
SELECT a, b, MAX(d), MIN(d) FROM t
WHERE (a > 6) AND (c = 3 OR c = 40) AND
(d = 11 OR d = 12)
GROUP BY a, b;
ERROR 70100: Query execution was interrupted
SET debug= '-d,bug30769515_QUERY_INTERRUPTED';
ALTER TABLE t DROP KEY k1;
ALTER TABLE t ADD KEY k1(a, b DESC, c, d DESC);
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET debug= '+d,bug30769515_QUERY_INTERRUPTED';
SELECT a, b, MAX(d), MIN(d) FROM t
WHERE (a > 6) AND (c = 3 OR c = 40) AND
(d = 11 OR d = 12)
GROUP BY a, b;
ERROR 70100: Query execution was interrupted
SET debug= '-d,bug30769515_QUERY_INTERRUPTED';
DROP TABLE t;
