include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TABLE t (a INT);
OPTIMIZE TABLE t;
Table	Op	Msg_type	Msg_text
test.t	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t	optimize	status	OK
include/sync_slave_sql_with_master.inc
[connection master]
DROP TABLE t;
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
