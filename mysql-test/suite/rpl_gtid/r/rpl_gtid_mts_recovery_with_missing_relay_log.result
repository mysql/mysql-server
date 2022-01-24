include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# 0. Create helper files for restarting the server with gaps
[connection slave]
include/suppress_messages.inc
# Connection 2 suppresses message <Recovery from master pos .* and file .* for channel ''*>.
# Connection 2 suppresses message <Error looking for file after .*slave-relay-bin.000007.*>.
# Connection 2 suppresses message <Slave: Failed to initialize the master info structure for channel .* its record may still be present in*>.
# Connection 2 suppresses message <Failed to create or recover replication info repositories.*>.
# Connection 2 suppresses message <Slave SQL for channel .* Slave failed to initialize relay log info structure from the repository, Error_code*>.
#
# 1. Check the replica can start after a restart that leaves gaps on the relay log
#    and relay log recovery is not used
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_begin.inc
[connection slave]
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers=2;
SET @@GLOBAL.replica_preserve_commit_order=OFF;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE TABLE d1.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
CREATE TABLE d2.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
FLUSH LOGS;
INSERT INTO d1.t VALUES (1, 'T1');
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO d2.t VALUES (2, 'Slave local');
[connection master1]
BEGIN;
INSERT INTO d1.t VALUES (2, 'T4');
[connection master]
INSERT INTO d2.t VALUES (1, 'T2');
INSERT INTO d2.t VALUES (2, 'T3');
[connection master1]
COMMIT;
[connection slave1]
include/rpl_stop_server.inc [server_number=2]
[connection slave]
include/rpl_start_server.inc [server_number=2 parameters: --skip_replica_start=1 --sync_source_info=1 --relay-log-recovery=0 --gtid_mode=ON --enforce-gtid-consistency --log-replica-updates]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d2.t, slave:d2.t]
include/diff_tables.inc [master:d1.t, slave:d1.t]
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_end.inc
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
#
# 2. Check the replica can start after a restart that leaves gaps on the relay log
#    and relay log recovery is used
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_begin.inc
[connection slave]
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers=2;
SET @@GLOBAL.replica_preserve_commit_order=OFF;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE TABLE d1.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
CREATE TABLE d2.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
FLUSH LOGS;
INSERT INTO d1.t VALUES (1, 'T1');
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO d2.t VALUES (2, 'Slave local');
[connection master1]
BEGIN;
INSERT INTO d1.t VALUES (2, 'T4');
[connection master]
INSERT INTO d2.t VALUES (1, 'T2');
INSERT INTO d2.t VALUES (2, 'T3');
[connection master1]
COMMIT;
[connection slave1]
include/rpl_stop_server.inc [server_number=2]
[connection slave]
include/rpl_start_server.inc [server_number=2 parameters: --skip_replica_start=1 --sync_source_info=1 --relay-log-recovery=1 --gtid_mode=ON --enforce-gtid-consistency --log-replica-updates]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d2.t, slave:d2.t]
include/diff_tables.inc [master:d1.t, slave:d1.t]
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_end.inc
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
#
# 3. Check the replica can't start after a restart that leaves gaps on the relay log
#    if GTID_MODE != ON even with relay log recovery
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_begin.inc
[connection slave]
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers=2;
SET @@GLOBAL.replica_preserve_commit_order=OFF;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE TABLE d1.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
CREATE TABLE d2.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
FLUSH LOGS;
INSERT INTO d1.t VALUES (1, 'T1');
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO d2.t VALUES (2, 'Slave local');
[connection master1]
BEGIN;
INSERT INTO d1.t VALUES (2, 'T4');
[connection master]
INSERT INTO d2.t VALUES (1, 'T2');
INSERT INTO d2.t VALUES (2, 'T3');
[connection master1]
COMMIT;
[connection slave1]
include/rpl_stop_server.inc [server_number=2]
[connection slave]
include/rpl_start_server.inc [server_number=2 parameters: --skip_replica_start=1 --sync_source_info=1 --relay-log-recovery=1 --gtid_mode=ON_PERMISSIVE --enforce-gtid-consistency --log-replica-updates]
START SLAVE;
ERROR HY000: Slave failed to initialize relay log info structure from the repository
include/rpl_restart_server.inc [server_number=2 parameters: --skip_replica_start=1 --sync_source_info=1 --relay-log-recovery=1 --gtid_mode=ON --enforce-gtid-consistency --log-replica-updates]
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d2.t, slave:d2.t]
include/diff_tables.inc [master:d1.t, slave:d1.t]
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_end.inc
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
#
# 4. Even if gaps exist, using  START SLAVE UNTIL SQL_AFTER_MTS_GAPS will throw
#    an warning stating that gaps were not calculated.
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_begin.inc
[connection slave]
include/stop_slave.inc
SET @@GLOBAL.replica_parallel_workers=2;
SET @@GLOBAL.replica_preserve_commit_order=OFF;
include/start_slave.inc
[connection master]
CREATE DATABASE d1;
CREATE DATABASE d2;
CREATE TABLE d1.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
CREATE TABLE d2.t (a INT PRIMARY KEY, name TEXT) ENGINE=INNODB;
FLUSH LOGS;
INSERT INTO d1.t VALUES (1, 'T1');
include/sync_slave_sql_with_master.inc
BEGIN;
INSERT INTO d2.t VALUES (2, 'Slave local');
[connection master1]
BEGIN;
INSERT INTO d1.t VALUES (2, 'T4');
[connection master]
INSERT INTO d2.t VALUES (1, 'T2');
INSERT INTO d2.t VALUES (2, 'T3');
[connection master1]
COMMIT;
[connection slave1]
include/rpl_stop_server.inc [server_number=2]
[connection slave]
include/rpl_start_server.inc [server_number=2 parameters: --skip_replica_start=1 --sync_source_info=1 --relay-log-recovery=0 --gtid_mode=ON --enforce-gtid-consistency --log-replica-updates]
START REPLICA UNTIL SQL_AFTER_MTS_GAPS;
Warnings:
Warning	4047	The until clause SQL_AFTER_MTS_GAPS is being used for channel '' when GTID_MODE = ON and SOURCE_AUTO_POSITION=1 meaning the server did not compute internally what gaps may exist in the relay log transaction execution. To close any execution gaps use either the SQL_BEFORE_GTIDS or SQL_AFTER_GTIDS until clause.
include/wait_for_slave_sql_to_stop.inc
include/stop_slave_io.inc
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:d2.t, slave:d2.t]
include/diff_tables.inc [master:d1.t, slave:d1.t]
# Sourcing extra/rpl_tests/rpl_generate_mts_gap_with_missing_relaylog_end.inc
[connection master]
DROP DATABASE d1;
DROP DATABASE d2;
include/sync_slave_sql_with_master.inc
include/rpl_reset.inc
#
# 5. Cleanup
include/rpl_end.inc
