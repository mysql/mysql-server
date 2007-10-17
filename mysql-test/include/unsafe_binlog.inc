# include/unsafe_binlog.inc
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Notes:
# 1. This test uses at least in case of InnoDB options
#     innodb_locks_unsafe_for_binlog = true
#     innodb_lock_timeout = 5
# 2. The comments/expectations refer to InnoDB.
#    They might be not valid for other storage engines.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was innodb_unsafe_binlog.test
#               main code went into include/unsafe_binlog.inc
#

#
# Test cases for bug#15650
#      DELETE with LEFT JOIN crashes server with innodb_locks_unsafe_for_binlog
#

--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
--enable_warnings
eval create table t1 (id int not null, f_id int not null, f int not null,
primary key(f_id, id)) engine = $engine_type;
eval create table t2 (id int not null,s_id int not null,s varchar(200),
primary key(id)) engine = $engine_type;
INSERT INTO t1 VALUES (8, 1, 3);
INSERT INTO t1 VALUES (1, 2, 1);
INSERT INTO t2 VALUES (1, 0, '');
INSERT INTO t2 VALUES (8, 1, '');
commit;
DELETE ml.* FROM t1 AS ml LEFT JOIN t2 AS mm ON (mm.id=ml.id)
WHERE mm.id IS NULL;
select ml.* from t1 as ml left join t2 as mm on (mm.id=ml.id)
where mm.id is null lock in share mode;
drop table t1,t2;

#
# Test case for unlock row bug where unlock releases all locks granted for
# a row. Only the latest lock should be released.
#

connect (a,localhost,root,,);
connect (b,localhost,root,,);
connection a;
eval create table t1(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t1 values(1,1),(2,2),(3,1),(4,2),(5,1),(6,2),(7,3);
commit;
set autocommit = 0;
select * from t1 lock in share mode;
update t1 set b = 5 where b = 1;
connection b;
set autocommit = 0;
#
# S-lock to records (2,2),(4,2), and (6,2) should not be released in a update
#
--error ER_LOCK_WAIT_TIMEOUT
select * from t1 where a = 2 and b = 2 for update;
connection a;
commit;
connection b;
commit;
drop table t1;
connection default;
disconnect a;
disconnect b;

#
# unlock row test
#

connect (a,localhost,root,,);
connect (b,localhost,root,,);
connection a;
eval create table t1(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t1 values(1,1),(2,2),(3,1),(4,2),(5,1),(6,2),(7,3);
commit;
set autocommit = 0;
update t1 set b = 5 where b = 1;
connection b;
set autocommit = 0;
#
# X-lock to record (7,3) should be released in a update
#
select * from t1 where a = 7 and b = 3 for update;
commit;
connection a;
commit;
drop table t1;
connection default;
disconnect a;
disconnect b;


#
# Consistent read should be used in following selects
#
# 1) INSERT INTO ... SELECT
# 2) UPDATE ... = ( SELECT ...)
# 3) CREATE ... SELECT

connect (a,localhost,root,,);
connect (b,localhost,root,,);
connection a;
eval create table t1(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t1 values (1,2),(5,3),(4,2);
eval create table t2(d int not null, e int, primary key(d)) engine = $engine_type;
insert into t2 values (8,6),(12,1),(3,1);
commit;
set autocommit = 0;
select * from t2 for update;
connection b;
set autocommit = 0;
insert into t1 select * from t2;
update t1 set b = (select e from t2 where a = d);
eval create table t3(d int not null, e int, primary key(d)) engine = $engine_type
select * from t2;
commit;
connection a;
commit;
connection default;
disconnect a;
disconnect b;
drop table t1, t2, t3;

#
# Consistent read should not be used if
#
# (a) isolation level is serializable OR
# (b) select ... lock in share mode OR
# (c) select ... for update
#
# in following queries:
#
# 1) INSERT INTO ... SELECT
# 2) UPDATE ... = ( SELECT ...)
# 3) CREATE ... SELECT

connect (a,localhost,root,,);
connect (b,localhost,root,,);
connect (c,localhost,root,,);
connect (d,localhost,root,,);
eval SET SESSION STORAGE_ENGINE = $engine_type;
connect (e,localhost,root,,);
connect (f,localhost,root,,);
connect (g,localhost,root,,);
eval SET SESSION STORAGE_ENGINE = $engine_type;
connect (h,localhost,root,,);
connect (i,localhost,root,,);
connect (j,localhost,root,,);
eval SET SESSION STORAGE_ENGINE = $engine_type;
connection a;
eval create table t1(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t1 values (1,2),(5,3),(4,2);
eval create table t2(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t2 values (8,6),(12,1),(3,1);
eval create table t3(d int not null, b int, primary key(d)) engine = $engine_type;
insert into t3 values (8,6),(12,1),(3,1);
eval create table t5(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t5 values (1,2),(5,3),(4,2);
eval create table t6(d int not null, e int, primary key(d)) engine = $engine_type;
insert into t6 values (8,6),(12,1),(3,1);
eval create table t8(a int not null, b int, primary key(a)) engine = $engine_type;
insert into t8 values (1,2),(5,3),(4,2);
eval create table t9(d int not null, e int, primary key(d)) engine = $engine_type;
insert into t9 values (8,6),(12,1),(3,1);
commit;
set autocommit = 0;
select * from t2 for update;
connection b;
set autocommit = 0;
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
--send
insert into t1 select * from t2;
connection c;
set autocommit = 0;
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
--send
update t3 set b = (select b from t2 where a = d);
connection d;
set autocommit = 0;
SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE;
--send
create table t4(a int not null, b int, primary key(a)) select * from t2;
connection e;
set autocommit = 0;
--send
insert into t5 (select * from t2 lock in share mode);
connection f;
set autocommit = 0;
--send
update t6 set e = (select b from t2 where a = d lock in share mode);
connection g;
set autocommit = 0;
--send
create table t7(a int not null, b int, primary key(a)) select * from t2 lock in share mode;
connection h;
set autocommit = 0;
--send
insert into t8 (select * from t2 for update);
connection i;
set autocommit = 0;
--send
update t9 set e = (select b from t2 where a = d for update);
connection j;
set autocommit = 0;
--send
create table t10(a int not null, b int, primary key(a)) select * from t2 for update;

connection b;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection c;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection d;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection e;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection f;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection g;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection h;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection i;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection j;
--error ER_LOCK_WAIT_TIMEOUT
reap;

connection a;
commit;

connection default;
disconnect a;
disconnect b;
disconnect c;
disconnect d;
disconnect e;
disconnect f;
disconnect g;
disconnect h;
disconnect i;
disconnect j;
drop table t1, t2, t3, t5, t6, t8, t9;
