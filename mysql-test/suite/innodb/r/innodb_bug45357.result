set session transaction isolation level read committed;
create table bug45357(a int, b int,key(b))engine=innodb;
insert into bug45357 values (25170,6122);
update bug45357 set a=1 where b=30131;
delete from bug45357 where b < 20996;
delete from bug45357 where b < 7001;
drop table bug45357;
