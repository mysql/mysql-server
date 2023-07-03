include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
SET @debug_saved= @@GLOBAL.DEBUG;
CREATE TABLE t (i INT);
SET GLOBAL DEBUG= "+d,wait_after_binlog_EOF";
INSERT INTO t VALUES (1);
INSERT INTO t VALUES (2);
FLUSH LOGS;
SET DEBUG_SYNC= 'now SIGNAL signal.rotate_finished';
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t,slave:t]
SET @@GLOBAL.DEBUG= @debug_saved;
SET DEBUG_SYNC= 'RESET';
DROP TABLE t;
include/rpl_end.inc
