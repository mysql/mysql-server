include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
include/suppress_messages.inc
# Connection 1 suppresses message <.*InnoDB redo logging is disabled.* All data could be lost in case of a server crash.*>.
# Connection 2 suppresses message <.*InnoDB redo logging is disabled.* All data could be lost in case of a server crash.*>.
#### 1. Run START REPLICA and replicate from some server.
[connection slave]
SET GLOBAL SUPER_READ_ONLY = ON;
include/start_slave.inc
[connection master]
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
include/sync_slave_sql_with_master.inc
include/gtid_step_assert.inc [count=1, only_count=1]
#### 2. Restart a replica while there is any MEMORY table defined.
[connection slave]
include/gtid_step_reset.inc
[connection master]
include/gtid_step_reset.inc
CREATE TABLE m1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=Memory;
include/sync_slave_sql_with_master.inc
include/assert.inc [Table m1 must exists]
include/stop_slave.inc
[connection slave]
include/start_slave.inc
[connection slave]
include/gtid_step_assert.inc [count=1, only_count=1]
include/rpl_restart_server.inc [server_number=2]
include/start_slave.inc
[connection master]
SELECT COUNT(*) FROM m1;
COUNT(*)
0
include/gtid_step_assert.inc [count=0, only_count=1]
INSERT INTO m1 values(1);
include/sync_slave_sql_with_master.inc
include/gtid_step_assert.inc [count=2, only_count=1]
#### 3. Restart a replica server while replica_open_temp_tables > 0
[connection slave]
include/stop_slave.inc
SET @@global.binlog_format = STATEMENT;
include/start_slave.inc
[connection master]
SET SESSION BINLOG_FORMAT = STATEMENT;
CREATE TEMPORARY TABLE t2(i INT);
include/sync_slave_sql_with_master.inc
[connection slave]
include/gtid_step_reset.inc
include/assert.inc [Replica_open_temp_tables shall be 1]
include/stop_slave.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until replica_open_temp_tables = 0.
[connection slave]
include/start_slave.inc
[connection slave]
include/gtid_step_assert.inc [count=0, only_count=0]
include/rpl_restart_server.inc [server_number=2 gtids=on]
include/gtid_step_reset.inc
include/start_slave.inc
[connection slave]
include/assert.inc [Replica_open_temp_tables shall be 0]
include/gtid_step_assert.inc [count=0, only_count=0]
#### 3 (b). when replica has open temp tables and we restart the
#### source server and reconnect replica. Replica generate extra
#### DROP TEMPORARY TABLE statements in this case
[connection slave]
include/stop_slave.inc
SET @@global.binlog_format = STATEMENT;
include/start_slave.inc
[connection master]
CREATE TEMPORARY TABLE t3(i INT);
include/sync_slave_sql_with_master.inc
include/save_binlog_position.inc
[connection slave]
include/stop_slave_io.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until replica_open_temp_tables = 0.
include/rpl_restart_server.inc [server_number=1 gtids=on]
[connection slave]
include/gtid_step_reset.inc
include/start_slave.inc
[connection master]
CREATE TABLE aux_table_to_sync (a INT);
DROP TABLE aux_table_to_sync;
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc [Gtid # Query/.*DROP.*t3.* # Gtid # Query/.*CREATE.*aux.* # Gtid # Query/.*DROP.*aux.*]
include/gtid_step_assert.inc [count=3, only_count=1]
[connection master]
#### 4. Using statement-based replication, create a temporary table
####    while not in super_read_only mode, and then disconnect while
####    in super_read_only mode.
SET @@global.super_read_only = 0;
SET @@session.binlog_format = STATEMENT;
CREATE TEMPORARY TABLE t1 (a INT);
include/gtid_step_reset.inc
include/save_binlog_position.inc
SET @@global.super_read_only = 1;
[connection slave]
include/assert_binlog_events.inc [Gtid # Query(.*DROP.*)]
include/gtid_step_assert.inc [count=1, only_count=0]
#### 5. ALTER INSTANCE AND CHECK GTID_EXCECUTED
[connection slave]
SET GLOBAL SUPER_READ_ONLY = ON;
include/gtid_step_reset.inc
ALTER INSTANCE ENABLE INNODB REDO_LOG;
ALTER INSTANCE DISABLE INNODB REDO_LOG;
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
ALTER INSTANCE RELOAD TLS;
include/gtid_step_assert.inc [count=0, only_count=0]
ALTER INSTANCE ROTATE INNODB MASTER KEY;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
#### 6(a) FLUSH STATEMENTS AND CHECK GTID EXCECUTED
[connection slave]
FLUSH LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH BINARY LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH TABLES;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH TABLES t1;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH ENGINE LOGS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH ERROR LOGS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH GENERAL LOGS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH HOSTS;
Warnings:
Warning	1287	'FLUSH HOSTS' is deprecated and will be removed in a future release. Please use TRUNCATE TABLE performance_schema.host_cache instead
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH PRIVILEGES;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH OPTIMIZER_COSTS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH RELAY LOGS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH SLOW LOGS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH STATUS;
include/gtid_step_assert.inc [count=1, only_count=0]
FLUSH USER_RESOURCES;
include/gtid_step_assert.inc [count=1, only_count=0]
#### 6(b) FLUSH STATEMENTS UNDER NO_WRITE_TO_BINLOG AND CHECK GTID EXCECUTED
[connection slave]
include/gtid_step_reset.inc
FLUSH LOCAL ENGINE LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG ERROR LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG GENERAL LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG HOSTS;
Warnings:
Warning	1287	'FLUSH HOSTS' is deprecated and will be removed in a future release. Please use TRUNCATE TABLE performance_schema.host_cache instead
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG PRIVILEGES;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG OPTIMIZER_COSTS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG RELAY LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG SLOW LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG STATUS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG USER_RESOURCES;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG TABLES;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG TABLES t1;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
FLUSH NO_WRITE_TO_BINLOG BINARY LOGS;
include/gtid_step_assert.inc [count=0, only_count=0]
#### 7. SET gtid_purged.
[connection slave]
SET @@GLOBAL.GTID_PURGED = '00000000-0000-0000-0000-000000000000:1-1000000000000000000';
include/assert.inc [Assert the GTID_PURGED value]
#### 8. SET gtid_next to a GTID, and then execute any of:
####  COMMIT
####  XA START
####  SET autocommit = 1
####  SET autocommit = 0
####  START TRANSACTION READ_ONLY; COMMIT;
include/gtid_step_reset.inc
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
XA START 'xa1';
XA END 'xa1';
XA PREPARE 'xa1';
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=1, only_count=1]
SET @@SESSION.AUTOCOMMIT = ON;
include/gtid_step_assert.inc [count=0, only_count=1]
SET @@SESSION.AUTOCOMMIT = OFF;
include/gtid_step_assert.inc [count=0, only_count=1]
[connection slave]
SET GLOBAL SUPER_READ_ONLY = ON;
include/gtid_step_reset.inc
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:2';
START TRANSACTION WITH CONSISTENT SNAPSHOT;
COMMIT;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=1, only_count=1]
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:3';
START TRANSACTION READ WRITE;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
SET GTID_NEXT = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:4';
START TRANSACTION READ ONLY;
COMMIT;
SET GTID_NEXT = 'AUTOMATIC';
include/gtid_step_assert.inc [count=1, only_count=1]
#### 9. The unusual DDL statements:
#### [CREATE | ALTER] [SPATIAL REFERENCE SYTEM | SERVER | LOGFILE GROUP].
[connection slave]
SET GLOBAL SUPER_READ_ONLY = OFF;
CREATE TABLE t_part(c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB
PARTITION BY RANGE(c1)
(PARTITION p0 VALUES LESS THAN (2),
PARTITION p1 VALUES LESS THAN (5),
PARTITION p3 VALUES LESS THAN (10),
PARTITION sp00 VALUES LESS THAN (15));
CREATE TABLE t_hash(c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t_nopart1(c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
CREATE TABLE t_part1(c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB
PARTITION BY RANGE(c1)
(PARTITION p1 VALUES LESS THAN (5),
PARTITION p2 VALUES LESS THAN (10));
SET GLOBAL SUPER_READ_ONLY = ON;
CREATE TABLE t_test(c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
CREATE SPATIAL REFERENCE SYSTEM 13001000 NAME 'TEST13001000 Long-Lat WGS 84' DEFINITION 'GEOGCS["Long-Lat WGS 84",DATUM["World Geodetic System 1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.017453292519943278,AUTHORITY["EPSG","9122"]],AXIS["Lon",EAST],AXIS["Lat",NORTH],AUTHORITY["EPSG","4326"]]';
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
CREATE SERVER s1 FOREIGN DATA WRAPPER mysql
OPTIONS (USER 'Remote', HOST '198.51.100.106', DATABASE 'test');
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
CREATE LOGFILE GROUP lg1 ADD UNDOFILE 'undo.dat' INITIAL_SIZE = 10M;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part TRUNCATE PARTITION p0, p1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_hash DISCARD TABLESPACE;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_hash IMPORT TABLESPACE;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_hash COALESCE PARTITION 1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_nopart1 REMOVE PARTITIONING;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part EXCHANGE PARTITION sp00 WITH TABLE t_nopart1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part REBUILD PARTITION p0;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part1 REORGANIZE PARTITION p1,p2 INTO (PARTITION p3 VALUES LESS THAN (300));
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part ADD PARTITION (PARTITION p3 VALUES LESS THAN (300));
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
ALTER TABLE t_part DROP PARTITION p3;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
#### 10. [UN]INSTALL [PLUGIN | COMPONENT]
include/gtid_step_reset.inc
INSTALL PLUGIN keyring_file SONAME 'keyring_file.so';
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
UNINSTALL PLUGIN keyring_file;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
#### 11. [OPTIMIZE | ANALYZE | REPAIR] TABLE
REPAIR TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	repair	Error	The MySQL server is running with the --super-read-only option so it cannot execute this statement
test.t1	repair	error	Corrupt
include/gtid_step_assert.inc [count=1, only_count=0]
ANALYZE TABLE t1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
OPTIMIZE TABLE t1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
#### 12. ALTER TABLE ... [OPTIMIZE | ANALYZE | REPAIR] PARTITION.
include/gtid_step_reset.inc
ALTER TABLE t_part OPTIMIZE PARTITION p0, p1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
ALTER TABLE t_part ANALYZE PARTITION p0;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
ALTER TABLE t_part REPAIR PARTITION p0, p1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
#### 13. Run GRANT, REVOKE, CREATE USER, DROP USER in a way that it
####     fails, e.g., create two users where the second already exists.
SET @@global.super_read_only = 0;
CREATE USER 'u1'@localhost IDENTIFIED BY '';
SET @@global.super_read_only = 1;
include/gtid_step_reset.inc
CREATE USER u1,u2;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
GRANT ALL ON *.* To u1,u2;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
REVOKE ALL ON *.* from u1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
DROP USER u1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
DROP USER u1,u2;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
#### 14. Run DML on mysql.gtid_executed, slave_master_info,
####     slave_relay_log_info, and slave_worker_info tables.
UPDATE mysql.slave_master_info SET Retry_count = Retry_count + 1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
UPDATE mysql.slave_relay_log_info SET Sql_delay = Sql_delay + 1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
UPDATE mysql.slave_worker_info SET Relay_log_pos = Relay_log_pos + 1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
UPDATE mysql.gtid_executed SET interval_end = interval_end + 1;
ERROR HY000: The MySQL server is running with the --super-read-only option so it cannot execute this statement
include/gtid_step_assert.inc [count=0, only_count=0]
#### 15. Cleanup
SET GLOBAL READ_ONLY = OFF;
SET GTID_NEXT = 'AUTOMATIC';
[connection master]
SET GLOBAL SUPER_READ_ONLY = OFF;
SET GLOBAL READ_ONLY = OFF;
DROP TABLE t1, m1;
include/rpl_end.inc
