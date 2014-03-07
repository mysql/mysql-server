include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create table t1 (a int, b int) engine=myisam;
insert into t1 values(1,1);
include/sync_slave_sql_with_master.inc
select * from t1;
a	b
1	1
delete from t1;
include/sync_slave_sql_with_master.inc
select * from t1;
a	b
drop table t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
