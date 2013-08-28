include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
use test;
drop table if exists mysqltest_foo;
drop table if exists mysqltest_bar;
create table mysqltest_foo (n int);
insert into mysqltest_foo values(4);
use test;
create table mysqltest_foo (n int);
insert into mysqltest_foo values(5);
create table mysqltest_bar (m int);
insert into mysqltest_bar values(15);
create table t1 (k int);
insert into t1 values(55);
select mysqltest_foo.n,mysqltest_bar.m,t1.k from mysqltest_foo,mysqltest_bar,t1;
n	m	k
4	15	55
drop table mysqltest_foo,mysqltest_bar,t1;
drop table mysqltest_foo,mysqltest_bar,t1;
include/rpl_end.inc
