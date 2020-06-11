create table t1 (a int not null,b int not null, primary key using BTREE (a)) engine=heap comment="testing heaps";
insert into t1 values(1,1),(2,2),(3,3),(4,4);
alter table t1 modify a int not null auto_increment, engine=myisam, comment="new myisam table";
select * from t1;
a	b
1	1
2	2
3	3
4	4
drop table t1;
create table t1 (a int not null) engine=heap;
insert into t1 values (869751),(736494),(226312),(802616),(728912);
select * from t1 where a > 736494;
a
802616
869751
alter table t1 add unique uniq_id using BTREE (a);
select * from t1 where a > 736494;
a
802616
869751
select * from t1 where a = 736494;
a
736494
select * from t1 where a=869751 or a=736494;
a
736494
869751
select * from t1 where a in (869751,736494,226312,802616);
a
226312
736494
802616
869751
alter table t1 engine=myisam;
explain select * from t1 where a in (869751,736494,226312,802616);
id	select_type	table	partitions	type	possible_keys	key	key_len	ref	rows	filtered	Extra
1	SIMPLE	t1	NULL	range	uniq_id	uniq_id	4	NULL	4	100.00	Using where; Using index
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`a` AS `a` from `test`.`t1` where (`test`.`t1`.`a` in (869751,736494,226312,802616))
drop table t1;
