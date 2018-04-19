include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/stop_slave.inc
CHANGE MASTER TO MASTER_DELAY= 10;
include/start_slave.inc
[connection master]
CREATE TABLE t1 (a varchar(50));
BEGIN;
INSERT INTO t1 VALUES ("1");
COMMIT;
include/sync_slave_sql_with_master.inc
[connection master]
include/assert.inc [Assert that the delay is relative to the immediate_commit_timestamp..]
include/assert.inc [Assert that the delay is not relative to the insert event timestamp or begining of transaction..]
DROP TABLE t1;
include/sync_slave_sql_with_master.inc
include/stop_slave_sql.inc
CHANGE MASTER TO MASTER_DELAY= 0;
include/start_slave_sql.inc
include/rpl_end.inc
