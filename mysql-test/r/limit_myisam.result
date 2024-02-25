#
# Bug #11747810: EXPLAIN SHOWS BOGUS VALUE FOR
#                'FILTERED' COLUMN FOR LIMIT QUERY
#
CREATE TABLE t1 (a INT, KEY (a)) ENGINE=Myisam;
INSERT INTO t1 VALUES (0),(1),(2),(3),(4),(5),(6),(7),(8),(9);
EXPLAIN SELECT * FROM t1 ORDER BY a LIMIT 1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index	NULL	a	5	NULL	1	100.00	Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` order by `test`.`t1`.`a` limit 1
DROP TABLE t1;
