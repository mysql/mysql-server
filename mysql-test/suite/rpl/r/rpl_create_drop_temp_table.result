include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TEMPORARY TABLE temp (i INT);
include/stop_slave_io.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until slave_open_temp_tables = 0.
include/rpl_stop_server.inc [server_number=1]
include/rpl_start_server.inc [server_number=1]
include/start_slave_io.inc
include/sync_slave_io_with_master.inc
include/assert.inc [Slave_open_temp_tables should be 0]
include/rpl_end.inc
