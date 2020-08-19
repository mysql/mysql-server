#
# Bug #31582383: QUICK_RANGE_SELECT::INIT_ROR_MERGED_SCAN ALWAYS
#                USES F_RDLCK
#
CREATE TABLE t1 (
id int auto_increment NOT NULL,
c1 int NOT NULL ,
c2 int NOT NULL,
c3 int NOT NULL,
PRIMARY KEY(id),
KEY c1 (c1),
KEY c2 (c2)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
INSERT INTO t1(c1, c2, c3) VALUES (1, 1, 1);
INSERT INTO t1 (c1, c2, c3) SELECT c1+1, c2+1, c3+1 FROM t1;
INSERT INTO t1 (c1, c2, c3) SELECT c1+8, c2+8, c3+8 FROM t1;
INSERT INTO t1 (c1, c2, c3) VALUES (1, 2, 888);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
EXPLAIN SELECT * FROM t1 WHERE c1 = 1 AND c2 = 2 FOR UPDATE;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index_merge	c1,c2	c1,c2	4,4	NULL	1	100.00	Using intersect(c1,c2); Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`c1` AS `c1`,`test`.`t1`.`c2` AS `c2`,`test`.`t1`.`c3` AS `c3` from `test`.`t1` where ((`test`.`t1`.`c2` = 2) and (`test`.`t1`.`c1` = 1))
SELECT * FROM t1 WHERE c1 = 1 AND c2 = 2 FOR UPDATE;
id	c1	c2	c3
6	1	2	888
DROP TABLE t1;
