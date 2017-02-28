include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
[connection slave]
SET @save_global_debug=@@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG= '+d,dbug.calculate_sbm_after_fake_rotate_log_event';
include/start_slave.inc
[connection master]
FLUSH LOGS;
[connection slave]
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than IO_THREAD's time.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET DEBUG_SYNC="now wait_for signal.reached";
include/assert.inc [Seconds Behind Master should be less than IO_THREAD's time.]
SET DEBUG_SYNC="now signal signal.done_sbm_calculation";
SET @@GLOBAL.DEBUG=@save_global_debug;
include/rpl_end.inc
