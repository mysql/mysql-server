--connection master

--eval create table t1 (a int key, b varchar(32), X $col_type) engine ndb
create table t2 (a int key, b varchar(32), c int unsigned) engine ndb;

begin;
insert into t1 values (1, "Master t1 a=1 at X=10", 10);
insert into t1 values (2, "Master t1 a=2 at X=10", 10);
insert into t1 values (3, "Master t1 a=3 at X=10", 10);
insert into t2 values (1, "Master t2 a=1 at c=10", 10);
insert into t2 values (2, "Master t2 a=2 at c=10", 10);
insert into t2 values (3, "Master t2 a=3 at c=10", 10);
commit;

--sync_slave_with_master
--connection slave

select * from t1 order by a;
select * from t2 order by a;

begin;
update t1 set b="Slave t1 a=1 at X=12", X=12 where a=1;
update t1 set b="Slave t1 a=2 at X=12", X=12 where a=2;
update t1 set b="Slave t1 a=3 at X=12", X=12 where a=3;
update t2 set b="Slave t2 a=1 at c=12", c=12 where a=1;
update t2 set b="Slave t2 a=2 at c=12", c=12 where a=2;
update t2 set b="Slave t2 a=3 at c=12", c=12 where a=3;
commit;

--connection master
begin;
update t1 set b="Master t1 a=1 at X=11", X=11 where a=1;
update t1 set b="Master t1 a=2 at X=11", X=11 where a=2;
update t1 set b="Master t1 a=3 at X=11", X=11 where a=3;
update t2 set b="Master t2 a=1 at c=11", c=11 where a=1;
update t2 set b="Master t2 a=2 at c=11", c=11 where a=2;
update t2 set b="Master t2 a=3 at c=11", c=11 where a=3;
commit;

--sync_slave_with_master
--connection slave

select * from t1 order by a;
select * from t2 order by a;

--connection master
begin;
update t1 set b="Master t1 a=1 at X=13", X=13 where a=1;
update t1 set b="Master t1 a=2 at X=13", X=13 where a=2;
update t1 set b="Master t1 a=3 at X=13", X=13 where a=3;
update t2 set b="Master t2 a=1 at c=13", c=13 where a=1;
update t2 set b="Master t2 a=2 at c=13", c=13 where a=2;
update t2 set b="Master t2 a=3 at c=13", c=13 where a=3;
commit;

--sync_slave_with_master
--connection slave

select * from t1 order by a;
select * from t2 order by a;

--connection master
drop table t1, t2;
