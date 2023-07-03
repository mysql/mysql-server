include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create user replicate@localhost identified by 'aaaaaaaaaaaaaaab',
replicate@127.0.0.1 identified by 'aaaaaaaaaaaaaaab';
grant replication slave on *.* to replicate@localhost;
grant replication slave on *.* to replicate@127.0.0.1;
include/start_slave.inc
create table t1(n int);
insert into t1 values(24);
include/sync_slave_sql_with_master.inc
select * from t1;
n
24
drop table t1;
delete from mysql.user where user="replicate";
include/rpl_end.inc
