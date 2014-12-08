include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (a INT);
INSERT INTO t1 VALUES (1);
include/sync_slave_sql_with_master.inc
include/assert.inc [The two statements should have been replicated, so that t1 contains value 1.]
DROP TABLE t1;
include/rpl_end.inc
