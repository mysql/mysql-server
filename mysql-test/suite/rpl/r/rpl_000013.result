include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create table t2(n int);
create temporary table t1 (n int);
insert into t1 values(1),(2),(3);
insert into t2 select * from t1;
create temporary table t1 (n int);
insert into t1 values (4),(5);
insert into t2 select * from t1 as t10;
insert into t2 values(6);
select * from t2;
n
1
2
3
4
5
6
show status like 'Slave_open_temp_tables';
Variable_name	Value
Slave_open_temp_tables	0
drop table if exists t1,t2;
Warnings:
Note	1051	Unknown table 'test.t1'
include/rpl_end.inc
