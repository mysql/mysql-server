include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/stop_slave.inc
set @save.replica_parallel_workers= @@global.replica_parallel_workers;
set @@global.replica_parallel_workers= 4;
include/start_slave.inc
create database d8;
create table d8.t8 (a int);
select round(rand()*8) into @var;
insert into d8.t8 values (@var);
create table d8.t7 (a int);
select round(rand()*8) into @var;
insert into d8.t7 values (@var);
create table d8.t6 (a int);
select round(rand()*8) into @var;
insert into d8.t6 values (@var);
create table d8.t5 (a int);
select round(rand()*8) into @var;
insert into d8.t5 values (@var);
create table d8.t4 (a int);
select round(rand()*8) into @var;
insert into d8.t4 values (@var);
create table d8.t3 (a int);
select round(rand()*8) into @var;
insert into d8.t3 values (@var);
create table d8.t2 (a int);
select round(rand()*8) into @var;
insert into d8.t2 values (@var);
create table d8.t1 (a int);
select round(rand()*8) into @var;
insert into d8.t1 values (@var);
create database d7;
create table d7.t8 (a int);
select round(rand()*8) into @var;
insert into d7.t8 values (@var);
create table d7.t7 (a int);
select round(rand()*8) into @var;
insert into d7.t7 values (@var);
create table d7.t6 (a int);
select round(rand()*8) into @var;
insert into d7.t6 values (@var);
create table d7.t5 (a int);
select round(rand()*8) into @var;
insert into d7.t5 values (@var);
create table d7.t4 (a int);
select round(rand()*8) into @var;
insert into d7.t4 values (@var);
create table d7.t3 (a int);
select round(rand()*8) into @var;
insert into d7.t3 values (@var);
create table d7.t2 (a int);
select round(rand()*8) into @var;
insert into d7.t2 values (@var);
create table d7.t1 (a int);
select round(rand()*8) into @var;
insert into d7.t1 values (@var);
create database d6;
create table d6.t8 (a int);
select round(rand()*8) into @var;
insert into d6.t8 values (@var);
create table d6.t7 (a int);
select round(rand()*8) into @var;
insert into d6.t7 values (@var);
create table d6.t6 (a int);
select round(rand()*8) into @var;
insert into d6.t6 values (@var);
create table d6.t5 (a int);
select round(rand()*8) into @var;
insert into d6.t5 values (@var);
create table d6.t4 (a int);
select round(rand()*8) into @var;
insert into d6.t4 values (@var);
create table d6.t3 (a int);
select round(rand()*8) into @var;
insert into d6.t3 values (@var);
create table d6.t2 (a int);
select round(rand()*8) into @var;
insert into d6.t2 values (@var);
create table d6.t1 (a int);
select round(rand()*8) into @var;
insert into d6.t1 values (@var);
create database d5;
create table d5.t8 (a int);
select round(rand()*8) into @var;
insert into d5.t8 values (@var);
create table d5.t7 (a int);
select round(rand()*8) into @var;
insert into d5.t7 values (@var);
create table d5.t6 (a int);
select round(rand()*8) into @var;
insert into d5.t6 values (@var);
create table d5.t5 (a int);
select round(rand()*8) into @var;
insert into d5.t5 values (@var);
create table d5.t4 (a int);
select round(rand()*8) into @var;
insert into d5.t4 values (@var);
create table d5.t3 (a int);
select round(rand()*8) into @var;
insert into d5.t3 values (@var);
create table d5.t2 (a int);
select round(rand()*8) into @var;
insert into d5.t2 values (@var);
create table d5.t1 (a int);
select round(rand()*8) into @var;
insert into d5.t1 values (@var);
create database d4;
create table d4.t8 (a int);
select round(rand()*8) into @var;
insert into d4.t8 values (@var);
create table d4.t7 (a int);
select round(rand()*8) into @var;
insert into d4.t7 values (@var);
create table d4.t6 (a int);
select round(rand()*8) into @var;
insert into d4.t6 values (@var);
create table d4.t5 (a int);
select round(rand()*8) into @var;
insert into d4.t5 values (@var);
create table d4.t4 (a int);
select round(rand()*8) into @var;
insert into d4.t4 values (@var);
create table d4.t3 (a int);
select round(rand()*8) into @var;
insert into d4.t3 values (@var);
create table d4.t2 (a int);
select round(rand()*8) into @var;
insert into d4.t2 values (@var);
create table d4.t1 (a int);
select round(rand()*8) into @var;
insert into d4.t1 values (@var);
create database d3;
create table d3.t8 (a int);
select round(rand()*8) into @var;
insert into d3.t8 values (@var);
create table d3.t7 (a int);
select round(rand()*8) into @var;
insert into d3.t7 values (@var);
create table d3.t6 (a int);
select round(rand()*8) into @var;
insert into d3.t6 values (@var);
create table d3.t5 (a int);
select round(rand()*8) into @var;
insert into d3.t5 values (@var);
create table d3.t4 (a int);
select round(rand()*8) into @var;
insert into d3.t4 values (@var);
create table d3.t3 (a int);
select round(rand()*8) into @var;
insert into d3.t3 values (@var);
create table d3.t2 (a int);
select round(rand()*8) into @var;
insert into d3.t2 values (@var);
create table d3.t1 (a int);
select round(rand()*8) into @var;
insert into d3.t1 values (@var);
create database d2;
create table d2.t8 (a int);
select round(rand()*8) into @var;
insert into d2.t8 values (@var);
create table d2.t7 (a int);
select round(rand()*8) into @var;
insert into d2.t7 values (@var);
create table d2.t6 (a int);
select round(rand()*8) into @var;
insert into d2.t6 values (@var);
create table d2.t5 (a int);
select round(rand()*8) into @var;
insert into d2.t5 values (@var);
create table d2.t4 (a int);
select round(rand()*8) into @var;
insert into d2.t4 values (@var);
create table d2.t3 (a int);
select round(rand()*8) into @var;
insert into d2.t3 values (@var);
create table d2.t2 (a int);
select round(rand()*8) into @var;
insert into d2.t2 values (@var);
create table d2.t1 (a int);
select round(rand()*8) into @var;
insert into d2.t1 values (@var);
create database d1;
create table d1.t8 (a int);
select round(rand()*8) into @var;
insert into d1.t8 values (@var);
create table d1.t7 (a int);
select round(rand()*8) into @var;
insert into d1.t7 values (@var);
create table d1.t6 (a int);
select round(rand()*8) into @var;
insert into d1.t6 values (@var);
create table d1.t5 (a int);
select round(rand()*8) into @var;
insert into d1.t5 values (@var);
create table d1.t4 (a int);
select round(rand()*8) into @var;
insert into d1.t4 values (@var);
create table d1.t3 (a int);
select round(rand()*8) into @var;
insert into d1.t3 values (@var);
create table d1.t2 (a int);
select round(rand()*8) into @var;
insert into d1.t2 values (@var);
create table d1.t1 (a int);
select round(rand()*8) into @var;
insert into d1.t1 values (@var);
include/sync_slave_sql_with_master.inc
include/sync_slave_sql_with_master.inc
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d8.t8, slave:d8.t8]
include/diff_tables.inc [master:d8.t7, slave:d8.t7]
include/diff_tables.inc [master:d8.t6, slave:d8.t6]
include/diff_tables.inc [master:d8.t5, slave:d8.t5]
include/diff_tables.inc [master:d8.t4, slave:d8.t4]
include/diff_tables.inc [master:d8.t3, slave:d8.t3]
include/diff_tables.inc [master:d8.t2, slave:d8.t2]
include/diff_tables.inc [master:d8.t1, slave:d8.t1]
include/diff_tables.inc [master:d7.t8, slave:d7.t8]
include/diff_tables.inc [master:d7.t7, slave:d7.t7]
include/diff_tables.inc [master:d7.t6, slave:d7.t6]
include/diff_tables.inc [master:d7.t5, slave:d7.t5]
include/diff_tables.inc [master:d7.t4, slave:d7.t4]
include/diff_tables.inc [master:d7.t3, slave:d7.t3]
include/diff_tables.inc [master:d7.t2, slave:d7.t2]
include/diff_tables.inc [master:d7.t1, slave:d7.t1]
include/diff_tables.inc [master:d6.t8, slave:d6.t8]
include/diff_tables.inc [master:d6.t7, slave:d6.t7]
include/diff_tables.inc [master:d6.t6, slave:d6.t6]
include/diff_tables.inc [master:d6.t5, slave:d6.t5]
include/diff_tables.inc [master:d6.t4, slave:d6.t4]
include/diff_tables.inc [master:d6.t3, slave:d6.t3]
include/diff_tables.inc [master:d6.t2, slave:d6.t2]
include/diff_tables.inc [master:d6.t1, slave:d6.t1]
include/diff_tables.inc [master:d5.t8, slave:d5.t8]
include/diff_tables.inc [master:d5.t7, slave:d5.t7]
include/diff_tables.inc [master:d5.t6, slave:d5.t6]
include/diff_tables.inc [master:d5.t5, slave:d5.t5]
include/diff_tables.inc [master:d5.t4, slave:d5.t4]
include/diff_tables.inc [master:d5.t3, slave:d5.t3]
include/diff_tables.inc [master:d5.t2, slave:d5.t2]
include/diff_tables.inc [master:d5.t1, slave:d5.t1]
include/diff_tables.inc [master:d4.t8, slave:d4.t8]
include/diff_tables.inc [master:d4.t7, slave:d4.t7]
include/diff_tables.inc [master:d4.t6, slave:d4.t6]
include/diff_tables.inc [master:d4.t5, slave:d4.t5]
include/diff_tables.inc [master:d4.t4, slave:d4.t4]
include/diff_tables.inc [master:d4.t3, slave:d4.t3]
include/diff_tables.inc [master:d4.t2, slave:d4.t2]
include/diff_tables.inc [master:d4.t1, slave:d4.t1]
include/diff_tables.inc [master:d3.t8, slave:d3.t8]
include/diff_tables.inc [master:d3.t7, slave:d3.t7]
include/diff_tables.inc [master:d3.t6, slave:d3.t6]
include/diff_tables.inc [master:d3.t5, slave:d3.t5]
include/diff_tables.inc [master:d3.t4, slave:d3.t4]
include/diff_tables.inc [master:d3.t3, slave:d3.t3]
include/diff_tables.inc [master:d3.t2, slave:d3.t2]
include/diff_tables.inc [master:d3.t1, slave:d3.t1]
include/diff_tables.inc [master:d2.t8, slave:d2.t8]
include/diff_tables.inc [master:d2.t7, slave:d2.t7]
include/diff_tables.inc [master:d2.t6, slave:d2.t6]
include/diff_tables.inc [master:d2.t5, slave:d2.t5]
include/diff_tables.inc [master:d2.t4, slave:d2.t4]
include/diff_tables.inc [master:d2.t3, slave:d2.t3]
include/diff_tables.inc [master:d2.t2, slave:d2.t2]
include/diff_tables.inc [master:d2.t1, slave:d2.t1]
include/diff_tables.inc [master:d1.t8, slave:d1.t8]
include/diff_tables.inc [master:d1.t7, slave:d1.t7]
include/diff_tables.inc [master:d1.t6, slave:d1.t6]
include/diff_tables.inc [master:d1.t5, slave:d1.t5]
include/diff_tables.inc [master:d1.t4, slave:d1.t4]
include/diff_tables.inc [master:d1.t3, slave:d1.t3]
include/diff_tables.inc [master:d1.t2, slave:d1.t2]
include/diff_tables.inc [master:d1.t1, slave:d1.t1]
include/sync_slave_sql_with_master.inc
create database d20;
create table d20.t1 (a int);
select round(rand()*8) into @var;
insert into d20.t1 values (@var);
create database d19;
create table d19.t1 (a int);
select round(rand()*8) into @var;
insert into d19.t1 values (@var);
create database d18;
create table d18.t1 (a int);
select round(rand()*8) into @var;
insert into d18.t1 values (@var);
create database d17;
create table d17.t1 (a int);
select round(rand()*8) into @var;
insert into d17.t1 values (@var);
create database d16;
create table d16.t1 (a int);
select round(rand()*8) into @var;
insert into d16.t1 values (@var);
create database d15;
create table d15.t1 (a int);
select round(rand()*8) into @var;
insert into d15.t1 values (@var);
create database d14;
create table d14.t1 (a int);
select round(rand()*8) into @var;
insert into d14.t1 values (@var);
create database d13;
create table d13.t1 (a int);
select round(rand()*8) into @var;
insert into d13.t1 values (@var);
create database d12;
create table d12.t1 (a int);
select round(rand()*8) into @var;
insert into d12.t1 values (@var);
create database d11;
create table d11.t1 (a int);
select round(rand()*8) into @var;
insert into d11.t1 values (@var);
create database d10;
create table d10.t1 (a int);
select round(rand()*8) into @var;
insert into d10.t1 values (@var);
create database d9;
create table d9.t1 (a int);
select round(rand()*8) into @var;
insert into d9.t1 values (@var);
create database d8;
create table d8.t1 (a int);
select round(rand()*8) into @var;
insert into d8.t1 values (@var);
create database d7;
create table d7.t1 (a int);
select round(rand()*8) into @var;
insert into d7.t1 values (@var);
create database d6;
create table d6.t1 (a int);
select round(rand()*8) into @var;
insert into d6.t1 values (@var);
create database d5;
create table d5.t1 (a int);
select round(rand()*8) into @var;
insert into d5.t1 values (@var);
create database d4;
create table d4.t1 (a int);
select round(rand()*8) into @var;
insert into d4.t1 values (@var);
create database d3;
create table d3.t1 (a int);
select round(rand()*8) into @var;
insert into d3.t1 values (@var);
create database d2;
create table d2.t1 (a int);
select round(rand()*8) into @var;
insert into d2.t1 values (@var);
create database d1;
create table d1.t1 (a int);
select round(rand()*8) into @var;
insert into d1.t1 values (@var);
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d20.t1, slave:d20.t1]
include/diff_tables.inc [master:d19.t1, slave:d19.t1]
include/diff_tables.inc [master:d18.t1, slave:d18.t1]
include/diff_tables.inc [master:d17.t1, slave:d17.t1]
include/diff_tables.inc [master:d16.t1, slave:d16.t1]
include/diff_tables.inc [master:d15.t1, slave:d15.t1]
include/diff_tables.inc [master:d14.t1, slave:d14.t1]
include/diff_tables.inc [master:d13.t1, slave:d13.t1]
include/diff_tables.inc [master:d12.t1, slave:d12.t1]
include/diff_tables.inc [master:d11.t1, slave:d11.t1]
include/diff_tables.inc [master:d10.t1, slave:d10.t1]
include/diff_tables.inc [master:d9.t1, slave:d9.t1]
include/diff_tables.inc [master:d8.t1, slave:d8.t1]
include/diff_tables.inc [master:d7.t1, slave:d7.t1]
include/diff_tables.inc [master:d6.t1, slave:d6.t1]
include/diff_tables.inc [master:d5.t1, slave:d5.t1]
include/diff_tables.inc [master:d4.t1, slave:d4.t1]
include/diff_tables.inc [master:d3.t1, slave:d3.t1]
include/diff_tables.inc [master:d2.t1, slave:d2.t1]
include/diff_tables.inc [master:d1.t1, slave:d1.t1]
include/sync_slave_sql_with_master.inc
set @@global.replica_parallel_workers= @save.replica_parallel_workers;
include/rpl_end.inc
