include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CHANGE REPLICATION SOURCE TO SOURCE_HEARTBEAT_PERIOD=5;
SET @save_old_replica_net_timeout=@@global.replica_net_timeout;
SET @@global.replica_net_timeout=10;
include/start_slave.inc
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG='+d,inject_2sec_sleep_when_skipping_an_event';
CREATE TABLE t1(i INT) engine=innodb;
INSERT INTO t1 VALUES (1);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
DROP TABLE t1;
include/start_slave.inc
include/sync_slave_sql_with_master.inc
SET GLOBAL DEBUG=@save_debug;
SET @@global.replica_net_timeout=@save_old_replica_net_timeout;
include/rpl_end.inc
