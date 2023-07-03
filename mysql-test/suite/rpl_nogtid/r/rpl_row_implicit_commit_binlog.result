include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#########################################################################
#                            CONFIGURATION
#########################################################################
CREATE TABLE tt_1 (ddl_case INT, PRIMARY KEY(ddl_case)) ENGINE = Innodb;
CREATE TABLE tt_2 (ddl_case INT, PRIMARY KEY(ddl_case)) ENGINE = Innodb;
CREATE TABLE nt_1 (ddl_case INT, PRIMARY KEY(ddl_case)) ENGINE = MyIsam;
INSERT INTO tt_1(ddl_case) VALUES(0);
INSERT INTO tt_2(ddl_case) VALUES(0);
#########################################################################
#                          CHECK IMPLICT COMMIT
#########################################################################
SET AUTOCOMMIT= 0;
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (43);
CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "UDF_EXAMPLE_LIB";
include/assert.inc [There should be a COMMIT event at the specified position (test#43)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE FUNCTION myfunc_int RETURNS INTEGER SONAME "LIB"
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (42);
DROP FUNCTION myfunc_int;
include/assert.inc [There should be a COMMIT event at the specified position (test#42)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP FUNCTION myfunc_int
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (41);
LOAD INDEX INTO CACHE nt_1 IGNORE LEAVES;
Table	Op	Msg_type	Msg_text
test.nt_1	preload_keys	status	OK
include/assert.inc [There should be a COMMIT event at the specified position (test#41)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (40);
LOAD INDEX INTO CACHE tt_1, tt_2 IGNORE LEAVES;
Table	Op	Msg_type	Msg_text
test.tt_1	preload_keys	note	The storage engine for the table doesn't support preload_keys
test.tt_2	preload_keys	note	The storage engine for the table doesn't support preload_keys
include/assert.inc [There should be a COMMIT event at the specified position (test#40)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (39);
ANALYZE TABLE nt_1;
Table	Op	Msg_type	Msg_text
test.nt_1	analyze	status	Table is already up to date
include/assert.inc [There should be a COMMIT event at the specified position (test#39)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ANALYZE TABLE nt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (38);
CHECK TABLE nt_1;
Table	Op	Msg_type	Msg_text
test.nt_1	check	status	OK
include/assert.inc [There should be a COMMIT event at the specified position (test#38)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (37);
OPTIMIZE TABLE nt_1;
Table	Op	Msg_type	Msg_text
test.nt_1	optimize	status	Table is already up to date
include/assert.inc [There should be a COMMIT event at the specified position (test#37)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; OPTIMIZE TABLE nt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (36);
REPAIR TABLE nt_1;
Table	Op	Msg_type	Msg_text
test.nt_1	repair	status	OK
include/assert.inc [There should be a COMMIT event at the specified position (test#36)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; REPAIR TABLE nt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (35);
LOCK TABLES tt_1 WRITE;
include/assert.inc [There should be a COMMIT event at the specified position (test#35)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (34);
UNLOCK TABLES;
include/assert.inc [There should be a COMMIT event at the specified position (test#34)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (33);
CREATE USER 'user'@'localhost';
include/assert.inc [There should be a COMMIT event at the specified position (test#33)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE USER 'user'@'localhost' IDENTIFIED WITH 'caching_sha2_password'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (32);
GRANT ALL ON *.* TO 'user'@'localhost';
include/assert.inc [There should be a COMMIT event at the specified position (test#32)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; GRANT ALL PRIVILEGES ON *.* TO 'user'@'localhost'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (31);
SET PASSWORD FOR 'user'@'localhost' = 'newpass';
include/assert.inc [There should be a COMMIT event at the specified position (test#31)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER USER 'user'@'localhost' IDENTIFIED WITH 'caching_sha2_password' AS '<non-deterministic-password-hash>'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (30);
REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'user'@'localhost';
include/assert.inc [There should be a COMMIT event at the specified position (test#30)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'user'@'localhost'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (29);
RENAME USER 'user'@'localhost' TO 'user_new'@'localhost';
include/assert.inc [There should be a COMMIT event at the specified position (test#29)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; RENAME USER 'user'@'localhost' TO 'user_new'@'localhost'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (28);
DROP USER 'user_new'@'localhost';
include/assert.inc [There should be a COMMIT event at the specified position (test#28)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP USER 'user_new'@'localhost'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (27);
CREATE EVENT evt ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR DO SELECT * FROM tt_1;
include/assert.inc [There should be a COMMIT event at the specified position (test#27)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE DEFINER=`root`@`localhost` EVENT evt ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR DO SELECT * FROM tt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (26);
ALTER EVENT evt COMMENT 'evt';
include/assert.inc [There should be a COMMIT event at the specified position (test#26)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER EVENT evt COMMENT 'evt'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (25);
DROP EVENT evt;
include/assert.inc [There should be a COMMIT event at the specified position (test#25)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP EVENT evt
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (24);
CREATE TRIGGER tr AFTER INSERT ON tt_1 FOR EACH ROW UPDATE tt_2 SET ddl_case = ddl_case WHERE ddl_case= NEW.ddl_case;
include/assert.inc [There should be a COMMIT event at the specified position (test#24)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER tr AFTER INSERT ON tt_1 FOR EACH ROW UPDATE tt_2 SET ddl_case = ddl_case WHERE ddl_case= NEW.ddl_case
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (23);
DROP TRIGGER tr;
include/assert.inc [There should be a COMMIT event at the specified position (test#23)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_2)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP TRIGGER tr
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (22);
CREATE FUNCTION fc () RETURNS VARCHAR(64) RETURN "fc";
include/assert.inc [There should be a COMMIT event at the specified position (test#22)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE DEFINER=`root`@`localhost` FUNCTION `fc`() RETURNS varchar(64) CHARSET utf8mb4
RETURN "fc"
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (21);
ALTER FUNCTION fc COMMENT 'fc';
include/assert.inc [There should be a COMMIT event at the specified position (test#21)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER FUNCTION fc COMMENT 'fc'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (20);
DROP FUNCTION fc;
include/assert.inc [There should be a COMMIT event at the specified position (test#20)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP FUNCTION fc
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (19);
CREATE PROCEDURE pc () UPDATE tt_2 SET ddl_case = ddl_case WHERE ddl_case= NEW.ddl_case;
include/assert.inc [There should be a COMMIT event at the specified position (test#19)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE DEFINER=`root`@`localhost` PROCEDURE `pc`()
UPDATE tt_2 SET ddl_case = ddl_case WHERE ddl_case= NEW.ddl_case
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (18);
ALTER PROCEDURE pc COMMENT 'pc';
include/assert.inc [There should be a COMMIT event at the specified position (test#18)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER PROCEDURE pc COMMENT 'pc'
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (17);
DROP PROCEDURE pc;
include/assert.inc [There should be a COMMIT event at the specified position (test#17)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP PROCEDURE pc
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (16);
CREATE VIEW v AS SELECT * FROM tt_1;
include/assert.inc [There should be a COMMIT event at the specified position (test#16)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v` AS SELECT * FROM tt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (15);
ALTER VIEW v AS SELECT * FROM tt_1;
include/assert.inc [There should be a COMMIT event at the specified position (test#15)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `v` AS SELECT * FROM tt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (14);
DROP VIEW v;
include/assert.inc [There should be a COMMIT event at the specified position (test#14)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP VIEW v
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (13);
CREATE INDEX ix ON tt_1(ddl_case);
include/assert.inc [There should be a COMMIT event at the specified position (test#13)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE INDEX ix ON tt_1(ddl_case)
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (12);
DROP INDEX ix ON tt_1;
include/assert.inc [There should be a COMMIT event at the specified position (test#12)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP INDEX ix ON tt_1
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (11);
CREATE TEMPORARY TABLE tt_xx (a int);
include/assert.inc [There should be a COMMIT event at the specified position (test#11)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (10);
ALTER TABLE tt_xx ADD COLUMN (b int);
include/assert.inc [There should be a COMMIT event at the specified position (test#10)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (9);
ALTER TABLE tt_xx RENAME new_tt_xx;
include/assert.inc [There should be a COMMIT event at the specified position (test#9)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (8);
DROP TEMPORARY TABLE IF EXISTS new_tt_xx;
include/assert.inc [There should be a COMMIT event at the specified position (test#8)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (7);
CREATE TABLE tt_xx (a int);
include/assert.inc [There should be a COMMIT event at the specified position (test#7)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; CREATE TABLE tt_xx (a int)
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (6);
ALTER TABLE tt_xx ADD COLUMN (b int);
include/assert.inc [There should be a COMMIT event at the specified position (test#6)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; ALTER TABLE tt_xx ADD COLUMN (b int)
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (5);
RENAME TABLE tt_xx TO new_tt_xx;
include/assert.inc [There should be a COMMIT event at the specified position (test#5)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; RENAME TABLE tt_xx TO new_tt_xx
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (4);
TRUNCATE TABLE new_tt_xx;
include/assert.inc [There should be a COMMIT event at the specified position (test#4)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; TRUNCATE TABLE new_tt_xx
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (3);
DROP TABLE IF EXISTS tt_xx, new_tt_xx;
Warnings:
Note	1051	Unknown table 'test.tt_xx'
include/assert.inc [There should be a COMMIT event at the specified position (test#3)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	use `test`; DROP TABLE IF EXISTS `new_tt_xx`,`tt_xx` /* generated by server */
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (2);
CREATE DATABASE db;
include/assert.inc [There should be a COMMIT event at the specified position (test#2)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	CREATE DATABASE db
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
INSERT INTO tt_1(ddl_case) VALUES (1);
DROP DATABASE IF EXISTS db;
include/assert.inc [There should be a COMMIT event at the specified position (test#1)]
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-
-b-b-b-b-b-b-b-b-b-b-b- >> << -b-b-b-b-b-b-b-b-b-b-b-
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.tt_1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
master-bin.000001	#	Query	#	#	DROP DATABASE IF EXISTS db
-e-e-e-e-e-e-e-e-e-e-e- >> << -e-e-e-e-e-e-e-e-e-e-e-

###################################################################################
#                               CHECK CONSISTENCY
###################################################################################
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:tt_1,slave:tt_1]
###################################################################################
#                                        CLEAN
###################################################################################
include/sync_slave_sql_with_master.inc
include/rpl_end.inc
