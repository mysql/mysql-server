include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
CREATE TEMPORARY TABLE TEMP1 (a int);
include/sync_slave_sql_with_master.inc

# Verify that the innodb table and slave_open_temp_table both indicate
# that there is an open temp table at slave.

include/assert.inc [Both INNODB_TEMP_TABLE_INFO and REPLICA_OPEN_TEMP_TABLES should show a temp table exists.]

# Verify that after STOP SLAVE the innodb table and slave_open_temp_table
# both indicate that there is an open temp table at slave.

include/stop_slave.inc
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until replica_open_temp_tables = 0.
include/assert.inc [Both INNODB_TEMP_TABLE_INFO and REPLICA_OPEN_TEMP_TABLES should show a temp table exists.]

# Verify that after CHANGE MASTER the innodb table and slave_open_temp_table
# both indicate that there is an open temp table at slave.

CHANGE REPLICATION SOURCE TO SOURCE_PORT= MASTER_PORT;
Warnings:
Warning	3022	This operation may not be safe when the slave has temporary tables. The tables will be kept open until the server restarts or until the tables are deleted by any replicated DROP statement. Suggest to wait until replica_open_temp_tables = 0.
include/assert.inc [Both INNODB_TEMP_TABLE_INFO and REPLICA_OPEN_TEMP_TABLES should show a temp table exists.]
include/start_slave.inc

# This is where the mismatch was found in the bug.
# Verify that the bug is fixed and hence we have equal values here.

include/assert.inc [count(*) from INNODB_TEMP_TABLE_INFO and REPLICA_OPEN_TEMP_TABLES should match.]
DROP TEMPORARY TABLE IF EXISTS TEMP1;
include/rpl_end.inc
