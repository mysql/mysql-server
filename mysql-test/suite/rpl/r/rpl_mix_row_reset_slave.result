include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
Master_User = 'root'
Master_Host = '127.0.0.1'
include/stop_slave.inc
change master to master_user='test';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
Master_User = 'test'
Master_Host = '127.0.0.1'
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
Master_User = 'test'
Master_Host = '127.0.0.1'
change master to master_user='root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
Master_User = 'root'
Master_Host = '127.0.0.1'
include/stop_slave.inc
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/start_slave.inc
create temporary table t1 (a int);
include/stop_slave.inc
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
reset master;
include/start_slave.inc
show status like 'slave_open_temp_tables';
Variable_name	Value
Slave_open_temp_tables	0
drop temporary table if exists t1;
include/stop_slave.inc
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/check_slave_no_error.inc
change master to master_user='impossible_user_name';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
start slave;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=1045]
include/stop_slave_sql.inc
change master to master_user='root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
include/check_slave_no_error.inc

Sync slave, else STOP SLAVE may complain about open temporary table.

[connection master]
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
change master to master_user='impossible_user_name';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
start slave;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=1045]
include/stop_slave_sql.inc
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
include/check_slave_no_error.inc
change master to master_user='root';
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
reset slave;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead

Sync slave, else STOP SLAVE may complain about open temporary table.

include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
reset slave all;
Warnings:
Warning	1287	'RESET SLAVE' is deprecated and will be removed in a future release. Please use RESET REPLICA instead
start slave;
ERROR HY000: The server is not configured as slave; fix in config file or with CHANGE MASTER TO
CHANGE MASTER TO MASTER_HOST= 'MASTER_HOST', MASTER_USER= 'MASTER_USER', MASTER_PORT= MASTER_PORT;
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
[connection master]
include/sync_slave_sql_with_master.inc
include/stop_slave.inc
include/rpl_reset_slave.inc
# Configure the slave with an empty user
CHANGE MASTER TO MASTER_USER='', MASTER_PORT=MASTER_PORT, MASTER_HOST='localhost';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
# Issue a RESET SLAVE instruction and try to start the slave IO thread that will error out
include/rpl_reset_slave.inc
START SLAVE IO_THREAD;
Warnings:
Warning	1287	'START SLAVE' is deprecated and will be removed in a future release. Please use START REPLICA instead
include/wait_for_slave_io_error.inc [errno=13117]
# Check the user is still empty
include/assert.inc [User_name in slave_master_info is empty]
# Cleanup
include/rpl_reset_slave.inc
CHANGE MASTER TO MASTER_HOST='127.0.0.1', MASTER_PORT=MASTER_PORT, MASTER_USER='root';
Warnings:
Note	1759	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	1760	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
include/start_slave.inc
call mtr.add_suppression("Slave I/O for channel '': Fatal error: Invalid .* username when attempting to connect to the master server*");
include/rpl_end.inc
