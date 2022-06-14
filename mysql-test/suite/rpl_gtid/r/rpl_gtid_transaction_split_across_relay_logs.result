include/rpl_init.inc [topology=1->2->3]
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection server_1]
CREATE TABLE t1 (i VARCHAR(8192)) ENGINE=MyISAM;
INSERT INTO t1 VALUES(repeat('a',8192)), (repeat('b', 8192));
[connection server_2]
SET @saved_debug=@@GLOBAL.debug;
SET GLOBAL debug="+d,stop_io_after_reading_write_rows_log_event";
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_to_stop.inc
SET GLOBAL debug=@saved_debug;
START SLAVE;
[connection server_3]
START SLAVE;
[connection server_1]
include/rpl_sync.inc
include/diff_tables.inc [server_1:t1, server_2:t1, server_3:t1]
[connection server_2]
include/assert.inc [SQL thread position should be updated to after the split transaction]
include/assert.inc [SQL thread file should be updated to after the split transaction]
[connection server_1]
DROP TABLE t1;
include/rpl_end.inc
