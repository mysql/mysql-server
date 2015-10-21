include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CALL mtr.add_suppression("Found 1 prepared XA transactions");
CREATE TABLE t1(c1 INT);
CREATE TABLE t2(c1 INT) ENGINE = MyISAM;
CREATE TABLE mem_t1(c1 INT PRIMARY KEY) ENGINE = MEMORY;
CREATE TABLE mem_t2(c1 INT PRIMARY KEY) ENGINE = MEMORY;
INSERT INTO mem_t1 VALUES(1),(2),(3);
INSERT INTO mem_t2 VALUES(1),(2),(3);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/rpl_restart_server.inc [server_number=1]
include/start_slave.inc
# Test case 1: verify DELETE FROM mem_xx can be binlogged correctly
#              in CREATE TABLE ... SELECT
[connection master]
include/save_binlog_position.inc
CREATE TABLE t3 SELECT * FROM mem_t1 UNION SELECT 10;
include/assert_binlog_events.inc [Anonymous_Gtid # Query/.*BEGIN # Query/.*DELETE.* # Query/.*COMMIT]
include/sync_slave_sql_with_master.inc
SELECT * FROM mem_t1;
c1
#
# Test case 2: verify DELETE FROM mem_xx can be binlogged correctly
#              in a DML on a MyISAM table
[connection master]
include/save_binlog_position.inc
INSERT INTO t2 SELECT * FROM mem_t2 UNION SELECT 10;
include/assert_binlog_events.inc [Anonymous_Gtid # Query/.*BEGIN # Query/.*DELETE.* # Query/.*COMMIT]
include/sync_slave_sql_with_master.inc
SELECT * FROM mem_t2;
c1
[connection master]
DROP TABLE t1, t2, t3, mem_t1, mem_t2;
include/rpl_end.inc
