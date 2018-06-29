include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Verify that CREATE TEMPORARY TABLE in a single STMT EVENT
# is not written into binlog after executing the event.
#
CREATE EVENT event1
ON SCHEDULE EVERY 2 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION NOT PRESERVE
DO BEGIN
CREATE TEMPORARY TABLE temp1 (i INT);
END;|
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a single STMT EVENT
# is not written into binlog after executing the event.
#
CREATE EVENT event1
ON SCHEDULE EVERY 2 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION NOT PRESERVE
DO BEGIN
DROP TEMPORARY TABLE temp1;
END;|
include/show_binlog_events.inc
CREATE TABLE t1 (i INT);
#
# Verify that CREATE TEMPORARY TABLE in a MULTI STMTs EVENT
# is not written into binlog after executing the event.
#
CREATE EVENT event1
ON SCHEDULE EVERY 2 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION NOT PRESERVE
DO BEGIN
CREATE TEMPORARY TABLE temp1 (i INT);
INSERT INTO temp1 VALUES (1);
INSERT INTO t1 VALUES (2);
END;|
include/show_binlog_events.inc
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
master-bin.000001	#	Query	#	#	BEGIN
master-bin.000001	#	Table_map	#	#	table_id: # (test.t1)
master-bin.000001	#	Write_rows	#	#	table_id: # flags: STMT_END_F
master-bin.000001	#	Xid	#	#	COMMIT /* XID */
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave_open_temp_tables shall be 0]
[connection master]
#
# Verify that DROP TEMPORARY TABLE in a MULTI STMTs EVENT
# is not written into binlog after executing the event.
#
CREATE EVENT event1
ON SCHEDULE EVERY 2 SECOND
STARTS NOW() ENDS NOW() + INTERVAL 1 SECOND
ON COMPLETION NOT PRESERVE
DO BEGIN
INSERT INTO temp1 VALUES (1);
DROP TEMPORARY TABLE temp1;
INSERT INTO t1 VALUES (4);
END;|
include/show_binlog_events.inc
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave_open_temp_tables shall be 0]
[connection master]
DROP TABLE t1;
include/rpl_end.inc
