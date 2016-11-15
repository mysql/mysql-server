include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
==== If server with replicate-same-server-id is slave of itself, it executes transactions twice ====
---- Initialize ----
[connection slave]
CREATE TABLE t1 (n INT);
RESET MASTER;
include/stop_slave.inc
CHANGE MASTER TO MASTER_PORT = SLAVE_PORT;
include/start_slave.inc
---- Test ----
INSERT INTO t1 VALUES (1);
SELECT * FROM t1;
n
1
1
---- Clean up ----
include/stop_slave.inc
DROP TABLE t1;
CHANGE MASTER TO MASTER_PORT = MASTER_PORT;
==== START SLAVE UNTIL works with replicate-same-server-id ====
---- Initialize ----
[connection master]
CREATE TABLE t1(n int);
CREATE TABLE t2(n int);
---- Test ----
[connection slave]
START SLAVE UNTIL MASTER_LOG_FILE = 'master-bin.000001', MASTER_LOG_POS = UNTIL_POS;
include/wait_for_slave_io_to_start.inc
include/wait_for_slave_sql_to_stop.inc
include/assert.inc [t1 should be replicated, t2 should not]
---- Clean up ----
[connection slave]
include/start_slave_sql.inc
[connection master]
DROP TABLE t1;
DROP TABLE t2;
include/rpl_end.inc
