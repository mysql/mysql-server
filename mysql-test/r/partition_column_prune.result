drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9;
create table t1 (a char, b char, c char)
partition by range columns(a,b,c)
( partition p0 values less than ('a','b','c'));
insert into t1 values ('a', NULL, 'd');
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
explain select * from t1 where a = 'a' AND c = 'd';
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p0	ALL	NULL	NULL	NULL	NULL	1	100.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b`,`test`.`t1`.`c` AS `c` from `test`.`t1` where ((`test`.`t1`.`c` = 'd') and (`test`.`t1`.`a` = 'a'))
select * from t1 where a = 'a' AND c = 'd';
a	b	c
a	NULL	d
drop table t1;
create table t1 (a int not null) partition by range columns(a) (
partition p0 values less than (10),
partition p1 values less than (20),
partition p2 values less than (30),
partition p3 values less than (40),
partition p4 values less than (50),
partition p5 values less than (60),
partition p6 values less than (70)
);
insert into t1 values (5),(15),(25),(35),(45),(55),(65);
insert into t1 values (5),(15),(25),(35),(45),(55),(65);
create table t2 (a int not null) partition by range(a) (
partition p0 values less than (10),
partition p1 values less than (20),
partition p2 values less than (30),
partition p3 values less than (40),
partition p4 values less than (50),
partition p5 values less than (60),
partition p6 values less than (70)
);
insert into t2 values (5),(15),(25),(35),(45),(55),(65);
insert into t2 values (5),(15),(25),(35),(45),(55),(65);
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
explain select * from t1 where a > 35 and a < 45;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p3,p4	ALL	NULL	NULL	NULL	NULL	4	25.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where ((`test`.`t1`.`a` > 35) and (`test`.`t1`.`a` < 45))
explain select * from t2 where a > 35 and a < 45;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t2	p3,p4	ALL	NULL	NULL	NULL	NULL	4	25.00	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t2`.`a` AS `a` from `test`.`t2` where ((`test`.`t2`.`a` > 35) and (`test`.`t2`.`a` < 45))
drop table t1, t2;
create table t1 (a int not null, b int not null ) 
partition by range columns(a,b) (
partition p01 values less than (2,10),
partition p02 values less than (2,20),
partition p03 values less than (2,30),
partition p11 values less than (4,10),
partition p12 values less than (4,20),
partition p13 values less than (4,30),
partition p21 values less than (6,10),
partition p22 values less than (6,20),
partition p23 values less than (6,30)
);
insert into t1 values (2,5), (2,15), (2,25),
(4,5), (4,15), (4,25), (6,5), (6,15), (6,25);
insert into t1 select * from t1;
analyze table t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
explain select * from t1 where a=2;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p01,p02,p03,p11	ALL	NULL	NULL	NULL	NULL	8	12.50	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where (`test`.`t1`.`a` = 2)
explain select * from t1 where a=4;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p11,p12,p13,p21	ALL	NULL	NULL	NULL	NULL	8	12.50	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where (`test`.`t1`.`a` = 4)
explain select * from t1 where a=2 and b < 22;
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	p01,p02,p03	ALL	NULL	NULL	NULL	NULL	6	16.67	Using where
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a`,`test`.`t1`.`b` AS `b` from `test`.`t1` where ((`test`.`t1`.`a` = 2) and (`test`.`t1`.`b` < 22))
drop table t1;
