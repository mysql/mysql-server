drop table if exists t1;
create table t1(n int not null, key(n)) delay_key_write = 1;
select count(distinct n) from t1;
count(distinct n)
100
explain select count(distinct n) from t1;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	n	n	4	NULL	21	100.00	Using index for group-by
Warnings:
Note	1003	/* select#1 */ select count(distinct `test`.`t1`.`n`) AS `count(distinct n)` from `test`.`t1`
drop table t1;
