include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t1 (c1 INT);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
[connection master]
INSERT INTO t1 VALUES (1);
[connection slave]
include/rpl_receive_event_count.inc [stop after 1 Query event]
include/start_slave_io.inc
include/include/rpl_skip_to_end_of_relay_log.inc
[connection master]
DROP TABLE t1;
include/rpl_end.inc
