--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Test for bug #55273 "FLUSH TABLE tm WITH READ LOCK for Merge table
--echo #                      causes assert failure".
--echo #
--disable_warnings
drop table if exists t1, t2, tm;
--enable_warnings
create table t1 (i int) engine = myisam;
create table t2 (i int) engine = myisam;
create table tm (i int) engine=merge union=(t1, t2);
insert into t1 values (1), (2);
insert into t2 values (3), (4);
--echo # The below statement should succeed and lock merge
--echo # table for read. Only merge table gets flushed and
--echo # not underlying tables.
flush tables tm with read lock;
select * from tm;
--echo # Check that underlying tables are locked.
select * from t1;
select * from t2;
unlock tables;
--echo # This statement should succeed as well and flush
--echo # all tables in the list.
flush tables tm, t1, t2 with read lock;
select * from tm;
--echo # Naturally, underlying tables should be locked in this case too.
select * from t1;
select * from t2;
unlock tables;
drop tables tm, t1, t2;
