--echo ***
--echo *** conflicting timestamps on insert
--echo ***

--connection master
delete from t1 where a=1;
--sync_slave_with_master

--echo *** slave - cause conflict
--connection slave
--source include/ndb_conflict_info_init.inc
insert into t1 values(1, "slave  X=3", 3, "1");

--echo *** master - cause conflict
--connection master
insert into t1 values(1, "master X=2", 2, "1");

--echo *** slave - check data
--sync_slave_with_master
--connection slave
--source include/ndb_conflict_info.inc
select * from t1 order by a;
