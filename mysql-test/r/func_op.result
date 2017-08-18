select 1+1,1-1,1+1*2,8/5,8%5,mod(8,5),mod(8,5)|0,-(1+1)*-2;
1+1	1-1	1+1*2	8/5	8%5	mod(8,5)	mod(8,5)|0	-(1+1)*-2
2	0	3	1.6000	3	3	3	4
explain select 1+1,1-1,1+1*2,8/5,8%5,mod(8,5),mod(8,5)|0,-(1+1)*-2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select (1 + 1) AS `1+1`,(1 - 1) AS `1-1`,(1 + (1 * 2)) AS `1+1*2`,(8 / 5) AS `8/5`,(8 % 5) AS `8%5`,(8 % 5) AS `mod(8,5)`,((8 % 5) | 0) AS `mod(8,5)|0`,(-((1 + 1)) * -(2)) AS `-(1+1)*-2`
select 1 | (1+1),5 & 3,bit_count(7) ;
1 | (1+1)	5 & 3	bit_count(7)
3	1	3
explain select 1 | (1+1),5 & 3,bit_count(7) ;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
Warnings:
Note	1003	/* select#1 */ select (1 | (1 + 1)) AS `1 | (1+1)`,(5 & 3) AS `5 & 3`,bit_count(7) AS `bit_count(7)`
select 1 << 32,1 << 63, 1 << 64, 4 >> 2, 4 >> 63, 1<< 63 >> 60;
1 << 32	1 << 63	1 << 64	4 >> 2	4 >> 63	1<< 63 >> 60
4294967296	9223372036854775808	0	1	0	8
select -1 | 0, -1 ^ 0, -1 & 0;
-1 | 0	-1 ^ 0	-1 & 0
18446744073709551615	18446744073709551615	0
select -1 | 1, -1 ^ 1, -1 & 1;
-1 | 1	-1 ^ 1	-1 & 1
18446744073709551615	18446744073709551614	1
select  1 | -1,  1 ^ -1,  1 & -1;
1 | -1	1 ^ -1	1 & -1
18446744073709551615	18446744073709551614	1
select  0 | -1,  0 ^ -1,  0 & -1;
0 | -1	0 ^ -1	0 & -1
18446744073709551615	18446744073709551615	0
select -1 >> 0, -1 << 0;
-1 >> 0	-1 << 0
18446744073709551615	18446744073709551615
select -1 >> 1, -1 << 1;
-1 >> 1	-1 << 1
9223372036854775807	18446744073709551614
drop table if exists t1,t2;
create table t1(a int);
create table t2(a int, b int);
insert into t1 values (1), (2), (3);
insert into t2 values (1, 7), (3, 7);
select t1.a, t2.a, t2.b, bit_count(t2.b) from t1 left join t2 on t1.a=t2.a;
a	a	b	bit_count(t2.b)
1	1	7	3
2	NULL	NULL	NULL
3	3	7	3
drop table t1, t2;
