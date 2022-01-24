include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/install_semisync.inc
call mtr.add_suppression(".*Semi-sync master failed on net_flush.*before waiting for replica reply.*");
call mtr.add_suppression(".*Timeout waiting for reply of binlog .* semi-sync up to file .*");
include/sync_slave_sql_with_master.inc
[connection slave]
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_SSL = 1;
include/start_slave.inc

# Try replicating CREATE TABLE and verify that everything is fine.

[connection master]
USE test;
CREATE TABLE table1 (a INT);
include/sync_slave_sql_with_master.inc

# STOP SLAVE and verify that replication works fine when slave resumes.

include/stop_slave_io.inc
[connection master]
INSERT INTO table1 VALUES(1);
[connection slave]
include/start_slave_io.inc

# RE-START SLAVE and execute on master, verify that replication works fine.

[connection master]
INSERT INTO table1 VALUES(2);
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave should have 2 rows(1,2)]

# Clean-up

[connection master]
DROP TABLE table1;
include/sync_slave_sql_with_master.inc
include/uninstall_semisync.inc
include/stop_slave.inc
CHANGE REPLICATION SOURCE TO SOURCE_SSL = 0;
include/rpl_end.inc
