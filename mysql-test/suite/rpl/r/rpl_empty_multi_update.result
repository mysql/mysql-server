include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/gtid_utils.inc
[connection master]
include/rpl_reset.inc
CREATE TABLE t1(c1 int) ENGINE=InnoDB;
CREATE TABLE t2(c1 int) ENGINE=InnoDB;
include/sync_slave_sql_with_master.inc
[connection master]
START TRANSACTION;
UPDATE t1,t2 SET t1.c1 = 0;
SAVEPOINT sp1;
UPDATE t1,t2 SET t1.c1 = 0;
SAVEPOINT sp2;
COMMIT;
include/sync_slave_sql_with_master.inc
include/assert.inc [The correct count of transactions is expected on the slave]
[connection master]
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2);
include/sync_slave_sql_with_master.inc
[connection master]
START TRANSACTION;
UPDATE t1,t2 SET t1.c1 = 0;
COMMIT;
include/sync_slave_sql_with_master.inc
include/assert.inc [The correct count of transactions is expected on the slave]
[connection master]
DROP TABLE t1;
DROP TABLE t2;
include/sync_slave_sql_with_master.inc
include/gtid_utils_end.inc
[connection master]
include/rpl_end.inc
