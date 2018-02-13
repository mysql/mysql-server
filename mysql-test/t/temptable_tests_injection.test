-- source include/have_debug.inc

CREATE TABLE t (c VARCHAR(8));

INSERT INTO t VALUES ('aabbccdd');

ANALYZE TABLE t;

SET debug = '+d,temptable_use_char_length';

SELECT DISTINCT * FROM t;

SET debug = '-d,temptable_use_char_length';

DROP TABLE t;
