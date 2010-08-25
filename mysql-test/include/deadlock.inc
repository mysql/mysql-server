# include/deadlock.inc
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Last update:
# 2006-07-26 ML refactoring + print when connection is switched
#               old name was t/innodb-deadlock.test
#               main code went into include/deadlock.inc
#

--echo # Establish connection con1 (user=root)
connect (con1,localhost,root,,);
--echo # Establish connection con2 (user=root)
connect (con2,localhost,root,,);

--disable_warnings
drop table if exists t1,t2;
--enable_warnings

#
# Testing of FOR UPDATE
#

--echo # Switch to connection con1
connection con1;
eval create table t1 (id integer, x integer) engine = $engine_type;
insert into t1 values(0, 0);
set autocommit=0;
SELECT * from t1 where id = 0 FOR UPDATE;

--echo # Switch to connection con2
connection con2;
set autocommit=0;

# The following query should hang because con1 is locking the record
--send
update t1 set x=2 where id = 0;
--sleep 2

--echo # Switch to connection con1
connection con1;
update t1 set x=1 where id = 0;
select * from t1;
commit;

--echo # Switch to connection con2
connection con2;
reap;
commit;

--echo # Switch to connection con1
connection con1;
select * from t1;
commit;

drop table t1;
#
# Testing of FOR UPDATE
#

--echo # Switch to connection con1
connection con1;
eval create table t1 (id integer, x integer) engine = $engine_type;
eval create table t2 (b integer, a integer) engine = $engine_type;
insert into t1 values(0, 0), (300, 300);
insert into t2 values(0, 10), (1, 20), (2, 30);
commit;
set autocommit=0;
select * from t2;
update t2 set a=100 where b=(SELECT x from t1 where id = b FOR UPDATE);
select * from t2;
select * from t1;

--echo # Switch to connection con2
connection con2;
set autocommit=0;

# The following query should hang because con1 is locking the record
--send
update t1 set x=2 where id = 0;
--sleep 2

--echo # Switch to connection con1
connection con1;
update t1 set x=1 where id = 0;
select * from t1;
commit;

--echo # Switch to connection con2
connection con2;
reap;
commit;

--echo # Switch to connection con1
connection con1;
select * from t1;
commit;

drop table t1, t2;
eval create table t1 (id integer, x integer) engine = $engine_type;
eval create table t2 (b integer, a integer) engine = $engine_type;
insert into t1 values(0, 0), (300, 300);
insert into t2 values(0, 0), (1, 20), (2, 30);
commit;

--echo # Switch to connection con1
connection con1;
select a,b from t2 UNION SELECT id, x from t1 FOR UPDATE;
select * from t2;
select * from t1;

--echo # Switch to connection con2
connection con2;

# The following query should hang because con1 is locking the record
update t2 set a=2 where b = 0;
select * from t2;
--send
update t1 set x=2 where id = 0;
--sleep 2

--echo # Switch to connection con1
connection con1;
update t1 set x=1 where id = 0;
select * from t1;
commit;

--echo # Switch to connection con2
connection con2;
reap;
commit;

--echo # Switch to connection con1
connection con1;
select * from t1;
commit;

# Cleanup
--echo # Switch to connection default + disconnect con1 and con2
connection default;
disconnect con1;
disconnect con2;
drop table t1, t2;

--echo End of 4.1 tests

#
# Bug#25164 create table `a` as select * from `A` hangs
#

set storage_engine=innodb;

--disable_warnings
drop table if exists a;
drop table if exists A;
--enable_warnings

create table A (c int);
insert into A (c) values (0);
--error 0,ER_LOCK_DEADLOCK,ER_TABLE_EXISTS_ERROR
create table a as select * from A;
drop table A;

--disable_warnings
drop table if exists a;
--enable_warnings

set storage_engine=default;

--echo End of 5.0 tests.
