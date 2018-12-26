--connection slave
--source suite/ndb_rpl/t/ndb_conflict_info_init.inc

--connection master

--eval create table t1 (a int, b varchar(32), X $col_type, d varchar(9), primary key (a, d)) engine ndb
create table t2 (a int key, b varchar(32), c int unsigned, d varchar(9)) engine ndb;

--echo ************************************************
--echo * some basic positive testing, i.e. no conflicts
--echo ************************************************
--connection master
--echo *** insert some data
begin;
insert into t1 values (1, "Master t1 a=1 at X=1", 1, '1');
insert into t1 values (2, "Master t1 a=2 at X=1", 1, '12');
insert into t1 values (3, "Master t1 a=3 at X=1", 1, '123');
insert into t2 values (1, "Master t2 a=1 at c=1", 1, '1');
insert into t2 values (2, "Master t2 a=2 at c=1", 1, '12');
insert into t2 values (3, "Master t2 a=3 at c=1", 1, '123');
commit;
--sync_slave_with_master

--echo *** master - update all
--connection master
begin;
update t1 set b="Master t1 X=2", X=X+1;
update t2 set b="Master t2 c=2", c=c+1;
commit;
--echo *** slave - check update all
--sync_slave_with_master
--connection slave
select * from t1 order by a, d;
select * from t2 order by a, d;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo *** master - update 2 rows in different tables
--connection master
update t1 set b="Master t1 a=2 at X=3", X=X+1 where a=2;
update t2 set b="Master t2 a=3 at c=3", c=c+1 where a=3;
--echo *** slave - check update 2 rows in different tables
--sync_slave_with_master
--connection slave
select * from t1 order by a, d;
select * from t2 order by a, d;
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo *** master - delete all
--connection master
delete from t1;
delete from t2;
--echo *** slave - check delete all
--sync_slave_with_master
--connection slave
select * from t1 order by a, d;
select * from t2 order by a, d;
--source suite/ndb_rpl/t/ndb_conflict_info.inc
--source suite/ndb_rpl/t/ndb_conflict_info_init.inc

--echo ************************
--echo * start conflict testing
--echo ************************
--echo *** master - insert some data
--connection master
begin;
insert into t1 values (1, "Master t1 a=1 at X=10", 10, '111');
insert into t1 values (2, "Master t1 a=2 at X=10", 10, '111222');
insert into t1 values (3, "Master t1 a=3 at X=10", 10, '111222333');
insert into t1 values (4, "Master t1 a=4 at X=10", 10, '111222333');
insert into t2 values (1, "Master t2 a=1 at c=10", 10, '111');
insert into t2 values (2, "Master t2 a=2 at c=10", 10, '111222');
insert into t2 values (3, "Master t2 a=3 at c=10", 10, '111222333');
insert into t2 values (4, "Master t2 a=4 at c=10", 10, '111222333');
commit;

--echo *** slave - check conflict info, no conflicts yet
--sync_slave_with_master
--connection slave
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo *** slave - check insert some data
select * from t1 order by a, d;
select * from t2 order by a, d;

--echo *** slave - update some data to cause conflicts
begin;
update t1 set b="Slave t1 a=1 at X=12", X=12 where a=1 and d='111';
update t1 set b="Slave t1 a=2 at X=12", X=12 where a=2 and d='111222';
update t1 set b="Slave t1 a=3 at X=12", X=12 where a=3 and d='111222333';
update t1 set b="Slave t1 a=4 at X=12", X=12 where a=4 and d='111222333';
update t2 set b="Slave t2 a=1 at c=12", c=12 where a=1;
update t2 set b="Slave t2 a=2 at c=12", c=12 where a=2;
update t2 set b="Slave t2 a=3 at c=12", c=12 where a=3;
update t2 set b="Slave t2 a=4 at c=12", c=12 where a=4;
commit;

--echo *** master - update some data to cause conflicts
--connection master
begin;
update t1 set b="Master t1 a=1 at X=11", X=11 where a=1 and d='111';
update t1 set b="Master t1 a=2 at X=11", X=11 where a=2 and d='111222';
update t1 set b="Master t1 a=3 at X=11", X=11 where a=3 and d='111222333';
delete from t1 where a = 4;
update t2 set b="Master t2 a=1 at c=11", c=11 where a=1;
update t2 set b="Master t2 a=2 at c=11", c=11 where a=2;
update t2 set b="Master t2 a=3 at c=11", c=11 where a=3;
delete from t2 where a = 4;
commit;

--echo *** slave - check conflict info, there should be some
--sync_slave_with_master
--connection slave
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo *** slave - check update some data that causes conflicts
select * from t1 order by a, d;
select * from t2 order by a, d;
--echo *** slave - correct some data to correspond to the master
update t1 set b="Slave t1 a=1 at X=11", X=11 where a=1 and d='111';
delete from t1 where a = 4;

--echo *** master - update some data, use higher timestamp
--connection master
begin;
update t1 set b="Master t1 a=1 at X=13", X=13 where a=1 and d='111';
update t1 set b="Master t1 a=2 at X=13", X=13 where a=2 and d='111222';
update t1 set b="Master t1 a=3 at X=13", X=13 where a=3 and d='111222333';
update t2 set b="Master t2 a=1 at c=13", c=13 where a=1;
update t2 set b="Master t2 a=2 at c=13", c=13 where a=2;
update t2 set b="Master t2 a=3 at c=13", c=13 where a=3;
commit;

--echo *** slave - check conflict info, change depends on calling test
--sync_slave_with_master
--connection slave
--source suite/ndb_rpl/t/ndb_conflict_info.inc

--echo *** slave - check higer timestamp
select * from t1 order by a, d;
select * from t2 order by a, d;

--connection master
drop table t1, t2;
