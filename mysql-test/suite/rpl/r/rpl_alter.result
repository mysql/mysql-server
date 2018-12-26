include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create database mysqltest;
create table mysqltest.t1 (n int PRIMARY KEY);
alter table mysqltest.t1 add m int;
insert into mysqltest.t1 values (1,2);
create table mysqltest.t2 (n int PRIMARY KEY);
insert into mysqltest.t2 values (45);
rename table mysqltest.t2 to mysqltest.t3, mysqltest.t1 to mysqltest.t2;
include/rpl_sync.inc
select * from mysqltest.t2;
n	m
1	2
select * from mysqltest.t3;
n
45
drop database mysqltest;
include/rpl_sync.inc
include/rpl_end.inc
