drop table if exists t1;
set session transaction isolation level read committed;
create table t1(a int not null) engine=innodb DEFAULT CHARSET=latin1;
create table t2 like t1;
insert into t2 values (1),(2),(3),(4),(5),(6),(7);
set autocommit=0;
begin;
replace into t1 select * from t2;
set session transaction isolation level read committed;
set autocommit=0;
delete from t2 where a=5;
commit;
delete from t2;
commit;
commit;
begin;
insert into t1 select * from t2;
set session transaction isolation level read committed;
set autocommit=0;
delete from t2 where a=5;
commit;
delete from t2;
commit;
commit;
select * from t1;
a
1
2
3
4
5
6
7
drop table t1;
drop table t2;
