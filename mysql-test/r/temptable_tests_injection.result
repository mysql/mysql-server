CREATE TABLE t (c VARCHAR(8));
INSERT INTO t VALUES ('aabbccdd');
ANALYZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	analyze	status	OK
SET debug = '+d,temptable_use_char_length';
SELECT DISTINCT * FROM t;
c
aabbccdd
SET debug = '-d,temptable_use_char_length';
DROP TABLE t;
