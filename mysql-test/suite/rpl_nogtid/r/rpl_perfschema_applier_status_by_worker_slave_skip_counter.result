include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
include/stop_slave_sql.inc
SET GLOBAL sql_slave_skip_counter= 4;
[connection master]
CREATE TABLE t1 (a int);
DROP TABLE t1;
[connection slave]
include/start_slave_sql.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/assert.inc [queueing_gtid was cleared after the gtid skipped transactions]
include/assert.inc [applying_gtid was cleared after the gtid skipped transactions]
include/assert.inc [Assert that the queued gtid is not clear even after the slave skipped the two transactions]
include/assert.inc [Assert that the applied gtid is clear after the slave skipped the two transactions]
include/rpl_end.inc
