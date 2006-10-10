# include/concurrent.inc
#
# Concurrent tests for transactional storage engines, mainly in UPDATE's
# Bug#3300
# Designed and tested by Sinisa Milivojevic, sinisa@mysql.com
#
# two non-interfering UPDATE's not changing result set
#
# The variable
#     $engine_type       -- storage engine to be tested
# has to be set before sourcing this script.
#
# Last update:
# 2006-08-02 ML test refactored
#               old name was t/innodb_concurrent.test
#               main code went into include/concurrent.inc
#

connection default;
eval SET SESSION STORAGE_ENGINE = $engine_type;
--disable_warnings
drop table if exists t1;
--enable_warnings
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
connect (thread1, localhost, mysqltest,,);
connection thread1;
eval SET SESSION STORAGE_ENGINE = $engine_type;
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",1);
connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send update t1 set eta=1+get_lock("hello",1)*0 where tipo=11;
sleep 1;
connection thread1;
begin;
update t1 set eta=2 where tipo=22;
select release_lock("hello");
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
select * from t1;
commit;
select * from t1;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;

#
# two UPDATE's running and one changing result set
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send update t1 set eta=1+get_lock("hello",10)*0 where tipo=1;
sleep 1;
connection thread1;
begin;
update t1 set tipo=1 where tipo=2;
select release_lock("hello");
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
select * from t1;
commit;
select * from t1;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;


#
# One UPDATE and one INSERT .... Monty's test
#

#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1 (a int not null, b int not null);
insert into t1 values (1,1),(2,1),(3,1),(4,1);
select get_lock("hello2",1000);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send update t1 set b=10+get_lock(concat("hello",a),1000)*0 where
a=2;
sleep 1;
connection thread1;
insert into t1 values (1,1);
select release_lock("hello2");
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
sleep 1;
connection thread2;
reap;
connection default;
drop table t1;

#
# one UPDATE changing result set and SELECT ... FOR UPDATE
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send select * from t1 where tipo=2 FOR UPDATE;
sleep 1;
connection thread1;
begin;
select release_lock("hello");
--error 1205
update t1 set tipo=1+get_lock("hello",10)*0 where tipo=2;
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
commit;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;

#
# one UPDATE not changing result set and SELECT ... FOR UPDATE
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send select * from t1 where tipo=2 FOR UPDATE;
sleep 1;
connection thread1;
begin;
select release_lock("hello");
--error 1205
update t1 set tipo=11+get_lock("hello",10)*0 where tipo=22;
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
commit;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;

#
# two SELECT ... FOR UPDATE
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send select * from t1 where tipo=2 FOR UPDATE;
sleep 1;
connection thread1;
begin;
select release_lock("hello");
--error 1205
select * from t1 where tipo=1 FOR UPDATE;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
commit;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;


#
# one UPDATE changing result set and DELETE
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send delete from t1 where tipo=2;
sleep 1;
connection thread1;
begin;
select release_lock("hello");
--error 1205
update t1 set tipo=1+get_lock("hello",10)*0 where tipo=2;
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
commit;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
drop table t1;


#
# one UPDATE not changing result set and DELETE
#
#connect (thread1, localhost, mysqltest,,);
connection thread1;
create table t1(eta int(11) not null, tipo int(11), c varchar(255));
insert into t1 values (7,7, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
insert into t1 values (8,8, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
insert into t1 values (10,1,"ccccccccccccccccccccccccccccccccccccccccccc");
insert into t1 values (20,2,"ddddddddddddddddddddddddddddddddddddddddddd");
insert into t1 values (30,1,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
insert into t1 values (40,2,"fffffffffffffffffffffffffffffffffffffffffff");
insert into t1 values (50,1,"ggggggggggggggggggggggggggggggggggggggggggg");
insert into t1 values (60,2,"hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
insert into t1 values (70,1,"iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii");
insert into t1 values (80,22,"jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
insert into t1 values (90,11,"kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk");
select get_lock("hello",10);
#connect (thread2, localhost, mysqltest,,);
connection thread2;
begin;
send delete from t1 where tipo=2;
sleep 1;
connection thread1;
begin;
select release_lock("hello");
update t1 set tipo=1+get_lock("hello",10)*0 where tipo=22;
select * from t1;
connection thread2;
reap;
select * from t1;
send commit;
connection thread1;
commit;
connection thread2;
reap;
select * from t1;
connection thread1;
select * from t1;
connection default;
sleep 1;
drop table t1;
disconnect thread1;
disconnect thread2;
