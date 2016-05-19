# Testing that purging rows in ndb_binlog_index doesn't block parallel inserts
--source include/have_ndb.inc
--source include/have_log_bin.inc
--source include/have_ndb_binlog_index_innodb.inc

connect (con1,localhost,root,,);
connect (con2,localhost,root,,);
connect (con3,localhost,root,,);
connect (con4,localhost,root,,);

connection default;

--echo First initialize the binlog
reset master;

--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings

--echo Test that binlog purging doesn't use table lock on the ndb_binlog_index table

--echo Put some real stuff in the Binlog.
create table test.t1 (pk integer primary key auto_increment, a varchar(1000)) engine=ndb;
insert into test.t1 values (NULL, repeat('BJC', 300));
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;
insert into test.t1 select NULL, a from test.t1;

--source include/wait_for_ndb_committed_to_binlog.inc

--echo Have at least 100 epoch rows
select count(*)/100 > 0 from mysql.ndb_binlog_index where File='./mysqld-bin.000001';

--echo Flush log to get onto the next file...
FLUSH LOGS;

connection con1;
--echo Pick one row from ndb_binlog_index
create temporary table my_ndb_binlog_index like mysql.ndb_binlog_index;
insert into my_ndb_binlog_index select * from mysql.ndb_binlog_index where File='./mysqld-bin.000001' limit 1;
select epoch from my_ndb_binlog_index into @epoch;
select orig_server_id from my_ndb_binlog_index into @orig_server_id;
select orig_epoch from my_ndb_binlog_index into @orig_epoch;

--echo Lock one row to block purge
start transaction;
select count(1) from mysql.ndb_binlog_index where epoch = @epoch and orig_server_id = @orig_server_id and orig_epoch = @orig_epoch and File='./mysqld-bin.000001' for update; 

connection con2;
--echo Check that there are no rows in the next file
select count(*)=0 from mysql.ndb_binlog_index where File='./mysqld-bin.000002';
--echo Start the purge that will get stuck on one row
--send PURGE BINARY LOGS TO 'mysqld-bin.000002'

connection con3;
--echo Wait 10 sec for purge to run into the row lock
sleep 10;
--echo Now we should still have some rows in the first file (since the purge is stuck)
select count(*)>0 from mysql.ndb_binlog_index where File='./mysqld-bin.000001';
--echo Insert some more rows
insert into test.t1 select NULL, a from test.t1;
--echo Wait for rows to be committed to binlog
--source include/wait_for_ndb_committed_to_binlog.inc
--echo Now we see rows in the next file (which means there is no table lock)
select count(*)>0 from mysql.ndb_binlog_index where File='./mysqld-bin.000002';

connection con1;
--echo Release the purge
commit;

connection con2;
--reap
--echo Purge done

connection con4;
--echo Now we should have no rows in the first file
select count(*)=0 from mysql.ndb_binlog_index where File='./mysqld-bin.000001';
--echo Now we still should see rows in the next file
select count(*)>0 from mysql.ndb_binlog_index where File='./mysqld-bin.000002';

connection default;
--echo cleanup
drop table test.t1;

--echo Done

