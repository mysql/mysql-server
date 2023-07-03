include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create table t1 (a int primary key) engine = myisam;
create table t4 (a int primary key);
insert into t1 values (1),(1);
Got one of the listed errors
insert into t4 values (1),(2);
include/sync_slave_sql_with_master.inc
show tables like 't1';
Tables_in_test (t1)
show tables like 't4';
Tables_in_test (t4)
t4
SELECT * FROM test.t4 ORDER BY a;
a
1
2
drop table t1;
include/sync_slave_sql_with_master.inc
select get_lock('crash_lock%20C', 10);
get_lock('crash_lock%20C', 10)
1
create table t2 (a int primary key) engine = myisam;
insert into t2 values(1),(2);
update t2 set a = a + if(a = 1, 10, get_lock('crash_lock%20C', 10));
kill <master_id>;
drop table t2;
insert into t4 values (3),(4);
include/sync_slave_sql_with_master.inc
SELECT * FROM test.t4 ORDER BY a;
a
1
2
3
4
DROP TABLE test.t4;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
