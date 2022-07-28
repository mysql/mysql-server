#
# Bug#33546083 - mysqld crash in Field::field_index
#
CREATE TABLE t1 (
pk int NOT NULL,
col_varchar_255_key varchar(255),
PRIMARY KEY (pk),
KEY col_varchar_255_key (col_varchar_255_key)
) ENGINE=InnoDB SECONDARY_ENGINE=mock;
ALTER TABLE t1 SECONDARY_LOAD;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
EXPLAIN SELECT (SELECT MAX(col_varchar_255_key ) FROM t1) AS field1
FROM t1 AS alias1
WHERE (alias1.col_varchar_255_key IN (SELECT 'f' UNION ALL SELECT 'r'))
GROUP BY field1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	PRIMARY	alias1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where; Using temporary; Using secondary engine MOCK
3	DEPENDENT SUBQUERY	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
4	DEPENDENT UNION	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
2	SUBQUERY	t1	NULL	ALL	NULL	NULL	NULL	NULL	1	100.00	Using secondary engine MOCK
Warnings:
Note	1003	/* select#1 */ select (/* select#2 */ select max(`test`.`t1`.`col_varchar_255_key`) from `test`.`t1`) AS `field1` from `test`.`t1` `alias1` where <in_optimizer>(`test`.`alias1`.`col_varchar_255_key`,<exists>(/* select#3 */ select 'f' having (<cache>(`test`.`alias1`.`col_varchar_255_key`) = <ref_null_helper>('f')) union all /* select#4 */ select 'r' having (<cache>(`test`.`alias1`.`col_varchar_255_key`) = <ref_null_helper>('r')))) group by `field1`
SELECT (SELECT MAX(col_varchar_255_key ) FROM t1) AS field1
FROM t1 AS alias1
WHERE (alias1.col_varchar_255_key IN (SELECT 'f' UNION ALL SELECT 'r'))
GROUP BY field1;
field1
DROP TABLE t1;
