SET eq_range_index_dive_limit=default;
SELECT @@eq_range_index_dive_limit;
@@eq_range_index_dive_limit
200
CREATE TABLE t1
(
/* Field names reflect value(rowid) distribution, st=STairs, swt= SaWTooth */
st_a int,
swt1a int,
swt2a int,
st_b int,
swt1b int,
swt2b int,
key sta_swt12a(st_a,swt1a,swt2a),
key sta_swt1a(st_a,swt1a),
key sta_swt2a(st_a,swt2a),
key sta_swt21a(st_a,swt2a,swt1a),
key st_a(st_a),
key stb_swt1a_2b(st_b,swt1b,swt2a),
key stb_swt1b(st_b,swt1b),
key st_b(st_b)
) ;
ALTER TABLE t1 DISABLE KEYS;
#
# Printing of many insert into t1 disabled.
#
ALTER TABLE t1 ENABLE KEYS;
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	Table is already up to date
#
# Run index_merge queries two times: 1) with index dives
#                                    2) with index statistics
#

explain 
select * from t1 
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index_merge	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a,stb_swt1a_2b	15,15	NULL	3	9.10	Using intersect(sta_swt12a,stb_swt1a_2b); Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt2b` = 1) and (`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt2a` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select * from t1 
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1 limit 5;
st_a	swt1a	swt2a	st_b	swt1b	swt2b
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1

explain 
select * from t1
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index_merge	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a,stb_swt1a_2b	15,15	NULL	3	91.01	Using intersect(sta_swt12a,stb_swt1a_2b); Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt2a` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select * from t1
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 limit 5;
st_a	swt1a	swt2a	st_b	swt1b	swt2b
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1

explain 
select * from t1
where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ref	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a	10	const,const	89	10.99	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select st_a, swt1a, st_b, swt1b from t1
where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1 limit 5;
st_a	swt1a	st_b	swt1b
1	1	1	1
1	1	1	1
1	1	1	1
1	1	1	1
1	1	1	1

SET eq_range_index_dive_limit=1;
SET SESSION DEBUG="+d,crash_records_in_range";

explain 
select * from t1 
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index_merge	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a,stb_swt1a_2b	15,15	NULL	3	9.00	Using intersect(sta_swt12a,stb_swt1a_2b); Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt2b` = 1) and (`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt2a` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select * from t1 
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 and swt2b=1 limit 5;
st_a	swt1a	swt2a	st_b	swt1b	swt2b
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1

explain 
select * from t1
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	index_merge	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a,stb_swt1a_2b	15,15	NULL	3	90.00	Using intersect(sta_swt12a,stb_swt1a_2b); Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt2a` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select * from t1
where st_a=1 and swt1a=1 and swt2a=1 and st_b=1 and swt1b=1 limit 5;
st_a	swt1a	swt2a	st_b	swt1b	swt2b
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1
1	1	1	1	1	1

explain 
select * from t1
where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1 limit 5;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	ref	sta_swt12a,sta_swt1a,sta_swt2a,sta_swt21a,st_a,stb_swt1a_2b,stb_swt1b,st_b	sta_swt12a	10	const,const	90	11.11	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`st_a` AS `st_a`,`test`.`t1`.`swt1a` AS `swt1a`,`test`.`t1`.`swt2a` AS `swt2a`,`test`.`t1`.`st_b` AS `st_b`,`test`.`t1`.`swt1b` AS `swt1b`,`test`.`t1`.`swt2b` AS `swt2b` from `test`.`t1` where ((`test`.`t1`.`swt1b` = 1) and (`test`.`t1`.`st_b` = 1) and (`test`.`t1`.`swt1a` = 1) and (`test`.`t1`.`st_a` = 1)) limit 5
select st_a, swt1a, st_b, swt1b from t1
where st_a=1 and swt1a=1 and st_b=1 and swt1b=1 and swt1b=1 limit 5;
st_a	swt1a	st_b	swt1b
1	1	1	1
1	1	1	1
1	1	1	1
1	1	1	1
1	1	1	1

SET eq_range_index_dive_limit=1;
SET SESSION DEBUG="+d,crash_records_in_range";
DROP TABLE t1;
