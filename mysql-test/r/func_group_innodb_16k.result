#
# Bug#13723054 CRASH WITH MIN/MAX AFTER QUICK_GROUP_MIN_MAX_SELECT::NEXT_MIN
#
CREATE TABLE t1(a BLOB, b VARCHAR(255) CHARSET LATIN1, c INT,
KEY(b, c, a(765))) ENGINE=INNODB;
INSERT INTO t1(a, b, c) VALUES ('', 'a', 0), ('', 'a', null), ('', 'a', 0),
('', 'a', null), ('', 'a', 0);
ANALYZE TABLE t1;
SELECT MIN(c) FROM t1 GROUP BY b;
MIN(c)
0
EXPLAIN SELECT MIN(c) FROM t1 GROUP BY b;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	b	b	263	NULL	2	100.00	Using index for group-by
Warnings:
Note	1003	/* select#1 */ select min(`test`.`t1`.`c`) AS `MIN(c)` from `test`.`t1` group by `test`.`t1`.`b`
DROP TABLE t1;
End of 5.6 tests
