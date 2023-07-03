include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
create table t1 (a int not null auto_increment primary key, b int, key(b));
INSERT INTO t1 (a) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(10);
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
INSERT INTO t1 (a) SELECT null FROM t1;
update t1 set b=(a/2*rand());
delete from t1 order by b limit 10000;
optimize table t1;
drop table t1;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
