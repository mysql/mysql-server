BEGIN;
INSERT INTO t0 VALUES(
repeat('a',10000),repeat('b',10000),repeat('c',10000),
repeat('d',10000),repeat('e',10000),repeat('f',10000));
UPDATE t0 SET a=substr(a,9999),b=substr(b,9999),c=substr(c,9999),
d=substr(d,9999),e=substr(e,9999),f=substr(f,9999);
ERROR HY000: Undo log record is too big.
UPDATE t0 SET a=substr(a,9999);
SELECT LENGTH(a) FROM t0;
LENGTH(a)
2
ROLLBACK;
SELECT * FROM t0;
a	b	c	d	e	f
DROP TABLE t0;
BEGIN;
INSERT INTO t0 VALUES(
repeat('a',10000),repeat('b',10000),repeat('c',10000),
repeat('d',10000),repeat('e',10000),repeat('f',10000));
UPDATE t0 SET a=substr(a,9999),b=substr(b,9999),c=substr(c,9999),
d=substr(d,9999),e=substr(e,9999),f=substr(f,9999);
ERROR HY000: Undo log record is too big.
UPDATE t0 SET a=substr(a,9999);
SELECT LENGTH(a) FROM t0;
LENGTH(a)
2
ROLLBACK;
SELECT * FROM t0;
a	b	c	d	e	f
DROP TABLE t0;
