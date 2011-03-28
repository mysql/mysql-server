--echo ***
--echo *** conflicting timestamps on update
--echo ***

--echo *** slave - cause conflict
--connection slave
--source include/ndb_conflict_info_init.inc
update t1 set b="slave  X=3", X=3 where a=1;

--echo *** master - cause conflict
--connection master
update t1 set b="master X=2", X=2 where a=1;
update t1 set b="master X=2", X=2 where a=2;

--echo *** slave - check data
--sync_slave_with_master
--connection slave
--source include/ndb_conflict_info.inc
select * from t1 order by a;
