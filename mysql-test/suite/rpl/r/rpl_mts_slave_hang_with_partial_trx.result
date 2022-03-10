include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG= "+d,stop_io_after_reading_write_rows_log_event";
[connection master]
CREATE TABLE t1 (c1 INT) ENGINE=InnoDB;
BEGIN;
INSERT INTO t1 (c1) VALUES (1);
COMMIT;
BEGIN;
INSERT INTO t1 (c1) VALUES (2);
COMMIT;
[connection slave]
include/wait_for_slave_io_to_stop.inc
SET GLOBAL DEBUG= @save_debug;
include/start_slave_io.inc
[connection master]
include/sync_slave_sql_with_master.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/diff_tables.inc [master:t1, slave:t1]
[connection master]
DROP TABLE t1;
include/rpl_end.inc
