create table bug53674(a int)engine=innodb;
insert into bug53674 values (1),(2);
start transaction;
select * from bug53674 for update;
a
1
2
select * from bug53674 where a=(select a from bug53674 where a > 1);
a
2
drop table bug53674;
