include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work");
include/stop_slave.inc
change master to master_auto_position=1;
include/start_slave.inc
[connection master]
CREATE TABLE t1(i INT) ENGINE=InnoDB;
FLUSH LOGS;
include/sync_slave_sql_with_master.inc
SET @save_debug=@@global.debug;
SET GLOBAL DEBUG='d,stop_io_after_reading_table_map_event';
[connection master]
INSERT INTO t1 VALUES (1);
[connection slave]
include/wait_for_slave_io_to_stop.inc
SET GLOBAL DEBUG= @save_debug;
include/start_slave_io.inc
[connection master]
include/sync_slave_sql_with_master.inc
[connection master]
DROP TABLE t1;
include/rpl_end.inc
