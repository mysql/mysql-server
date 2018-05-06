include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
DROP TABLE IF EXISTS t1;
Warnings:
Note	1051	Unknown table 'test.t1'
CREATE TABLE t1(a varchar(40000))ENGINE=INNODB CHARACTER SET latin1;
INSERT INTO t1 VALUES (REPEAT('a',37000));
INSERT INTO t1 VALUES (REPEAT('b',32543));
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1,slave:t1]
DROP TABLE t1;
include/rpl_end.inc
