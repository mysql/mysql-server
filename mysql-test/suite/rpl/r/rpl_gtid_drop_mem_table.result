include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection master]
CREATE TABLE mem_t1 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t2 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t3 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t4 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t5 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t6 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t7 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t8 (c1 INT) ENGINE= MEMORY;
INSERT INTO mem_t1 VALUES (1), (2), (3), (4);
INSERT INTO mem_t2 VALUES (1), (2), (3), (4);
INSERT INTO mem_t3 VALUES (1), (2), (3), (4);
INSERT INTO mem_t4 VALUES (1), (2), (3), (4);
INSERT INTO mem_t5 VALUES (1), (2), (3), (4);
INSERT INTO mem_t6 VALUES (1), (2), (3), (4);
INSERT INTO mem_t7 VALUES (1), (2), (3), (4);
INSERT INTO mem_t8 VALUES (1), (2), (3), (4);
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/rpl_restart_server.inc [server_number=1]
include/start_slave.inc
[connection master]
include/save_binlog_position.inc
INSERT INTO mem_t1 VALUES (1), (2), (3), (4);
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid # !Begin # !Insert # !Commit]
include/save_binlog_position.inc
ALTER TABLE mem_t2 ADD COLUMN (c2 INT);
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid # !Q(ALTER.*)]
include/save_binlog_position.inc
CREATE TABLE mem_t9 LIKE mem_t3;
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid # !Q(CREATE.*)]
include/save_binlog_position.inc
BEGIN;
INSERT INTO mem_t4 VALUES (1), (2), (3), (4);
INSERT INTO mem_t5 VALUES (1), (2), (3), (4);
COMMIT;
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid # !Begin # !Insert # !Commit #Gtid # !Q(TRUNCATE.*) #Gtid # !Begin # !Insert # !Commit]
include/save_binlog_position.inc
UPDATE mem_t6, mem_t7 SET mem_t6.c1 = mem_t6.c1 + 1, mem_t7.c1 = mem_t7.c1 + 1;
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid # !Q(TRUNCATE.*)]
include/save_binlog_position.inc
SET @@session.gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
INSERT INTO mem_t8 VALUES (1), (2), (3), (4);
SET @@session.gtid_next = 'AUTOMATIC';
include/sync_slave_sql_with_master.inc
[connection master]
include/assert_binlog_events.inc [#Gtid # !Q(TRUNCATE.*) #Gtid/(.*'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1')# !Begin # !Insert # !Commit]
DROP TABLE mem_t1, mem_t2, mem_t3, mem_t4, mem_t5, mem_t6, mem_t7, mem_t8, mem_t9;
include/rpl_reset.inc
[connection master]
CREATE TABLE mem_t1 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t2 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t3 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t4 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t5 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t6 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t7 (c1 INT) ENGINE= MEMORY;
CREATE TABLE mem_t8 (c1 INT) ENGINE= MEMORY;
INSERT INTO mem_t1 VALUES (1), (2), (3), (4);
INSERT INTO mem_t2 VALUES (1), (2), (3), (4);
INSERT INTO mem_t3 VALUES (1), (2), (3), (4);
INSERT INTO mem_t4 VALUES (1), (2), (3), (4);
INSERT INTO mem_t5 VALUES (1), (2), (3), (4);
INSERT INTO mem_t6 VALUES (1), (2), (3), (4);
INSERT INTO mem_t7 VALUES (1), (2), (3), (4);
INSERT INTO mem_t8 VALUES (1), (2), (3), (4);
include/sync_slave_sql_with_master.inc
include/rpl_restart_server.inc [server_number=2]
include/start_slave.inc
include/save_binlog_position.inc
[connection master]
[connection slave]
[connection master]
INSERT INTO mem_t1 VALUES (1), (2), (3), (4);
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
include/save_binlog_position.inc
[connection master]
ALTER TABLE mem_t2 ADD COLUMN (c2 INT);
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
include/save_binlog_position.inc
[connection master]
CREATE TABLE mem_t9 LIKE mem_t3;
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
include/save_binlog_position.inc
[connection master]
BEGIN;
INSERT INTO mem_t4 VALUES (1), (2), (3), (4);
INSERT INTO mem_t5 VALUES (1), (2), (3), (4);
COMMIT;
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
include/save_binlog_position.inc
[connection master]
CREATE FUNCTION f () RETURNS INT BEGIN
INSERT INTO mem_t6 VALUES (1);
RETURN 2;
END|
INSERT INTO mem_t7 VALUES (f());
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
include/save_binlog_position.inc
[connection master]
SET @@session.gtid_next = 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa:1';
INSERT INTO mem_t8 VALUES (1), (2), (3), (4);
SET @@session.gtid_next = 'AUTOMATIC';
include/sync_slave_sql_with_master.inc
[connection slave]
include/assert_binlog_events.inc
[connection master]
DROP FUNCTION f;
DROP TABLE mem_t1, mem_t2, mem_t3, mem_t4, mem_t5, mem_t6, mem_t7, mem_t8, mem_t9;
include/rpl_end.inc
