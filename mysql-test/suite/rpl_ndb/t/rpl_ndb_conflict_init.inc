--echo *** reset and init
--disable_query_log
--disable_result_log
--connection slave
stop slave;
delete from t1;
--error 0,1146
delete from `test`.`t1$EX`;

--connection master
delete from t1;
reset master;

--connection slave
reset slave;
start slave;
--source include/ndb_conflict_info_init.inc
--disable_query_log
--disable_result_log

--connection master
insert into t1 values (1, "master X=1", 1, "1");
insert into t1 values (2, "master X=1", 1, "2");
insert into t1 values (3, "master X=1", 1, "3");

--sync_slave_with_master
--enable_query_log
--enable_result_log
