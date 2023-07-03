include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/rpl_reset.inc
[connection master]
CREATE TABLE t1 (c1 INT);
CREATE TABLE t2 (c1 INT);
include/sync_slave_sql_with_master.inc
LOCK TABLE t2 READ, performance_schema.threads READ;
[connection master]
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (1);
COMMIT;
[connection slave]
include/assert.inc [SOURCE_POS_WAIT returns -1 if the timeout has been exceeded.]
include/assert.inc [Table t2 on the slave has no records]
UNLOCK TABLE;
include/assert.inc [SOURCE_POS_WAIT succeeds.]
include/diff_tables.inc [master:test.t1,slave:test.t1]
include/diff_tables.inc [master:test.t2,slave:test.t2]
[connection master]
DROP TABLE t1, t2;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
include/rpl_end.inc
