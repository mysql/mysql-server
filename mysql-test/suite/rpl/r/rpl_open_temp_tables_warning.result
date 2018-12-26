include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TEMPORARY TABLE t1 (a INT);
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave_open_temp_tables is correctly set to 1 after creating a temp table]
include/stop_slave.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until slave_open_temp_tables = 0.
include/assert.inc [Slave_open_temp_tables is correctly set to 1 after stopping the replication thread]
include/start_slave.inc
[connection master]
DROP TEMPORARY TABLE t1;
include/sync_slave_sql_with_master.inc
include/assert.inc [Slave_open_temp_tables is correctly set to 0 after dropping the temporary table]
include/stop_slave.inc
include/rpl_end.inc
