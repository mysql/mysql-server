include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
#
# Test replicate-ignore-db=db1 filter
#
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --replicate-ignore-db=db1 --skip_slave_start=FALSE]
[connection master]
#
# Execute 'CREATE DATABASE db1' on Master.
#
CREATE DATABASE db1;
#
# Sync with Slave (using gtid values)
#
include/sync_slave_sql_with_master.inc
#
# Check that even after sync is completed 'db1' does not exists
# on Slave which implies that 'CREATE DATABASE db1' is filtered
# out.
#
USE db1;
ERROR 42000: Unknown database 'db1'
#
# Execute other database commands (ALTER/DROP) on Master.
#
[connection master]
ALTER DATABASE db1 CHARACTER SET latin1;
DROP DATABASE db1;
#
# Check that we are able to sync with slave successfully
# which implies that those commands are filtered out.
# If they were executed by Slave, that will break replication
# as we do not have 'db1' database on Slave.
include/sync_slave_sql_with_master.inc
#
# Test replicate-do-db=db2 filter
#
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --replicate-do-db=db2 --skip_slave_start=FALSE]
[connection master]
#
# Execute 'CREATE DATABASE db1' on Master.
#
CREATE DATABASE db1;
#
# Sync with Slave (using gtid values)
#
include/sync_slave_sql_with_master.inc
#
# Check that even after sync is completed 'db1' does not exists
# on Slave which implies that 'CREATE DATABASE db1' is filtered
# out.
#
USE db1;
ERROR 42000: Unknown database 'db1'
#
# Execute other database commands (ALTER/DROP) on Master.
#
[connection master]
ALTER DATABASE db1 CHARACTER SET latin1;
DROP DATABASE db1;
#
# Check that we are able to sync with slave successfully
# which implies that those commands are filtered out.
# If they were executed by Slave, that will break replication
# as we do not have 'db1' database on Slave.
include/sync_slave_sql_with_master.inc
#
# Test replicate-wild-ignore-table=db1.% filter
#
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --replicate-wild-ignore-table=db1.% --skip_slave_start=FALSE]
[connection master]
#
# Execute 'CREATE DATABASE db1' on Master.
#
CREATE DATABASE db1;
#
# Sync with Slave (using gtid values)
#
include/sync_slave_sql_with_master.inc
#
# Check that even after sync is completed 'db1' does not exists
# on Slave which implies that 'CREATE DATABASE db1' is filtered
# out.
#
USE db1;
ERROR 42000: Unknown database 'db1'
#
# Execute other database commands (ALTER/DROP) on Master.
#
[connection master]
ALTER DATABASE db1 CHARACTER SET latin1;
DROP DATABASE db1;
#
# Check that we are able to sync with slave successfully
# which implies that those commands are filtered out.
# If they were executed by Slave, that will break replication
# as we do not have 'db1' database on Slave.
include/sync_slave_sql_with_master.inc
#
# Test replicate-wild-ignore-table=db1.% and --replicate-do-db=db2
# filter combination
include/rpl_restart_server.inc [server_number=2 gtids=on parameters: --replicate-do-db=db2 --replicate-wild-ignore-table=db1.t% --skip_slave_start=FALSE]
[connection master]
#
# Execute 'CREATE DATABASE db1' on Master.
#
CREATE DATABASE db1;
#
# Sync with Slave (using gtid values)
#
include/sync_slave_sql_with_master.inc
#
# Check that even after sync is completed 'db1' does not exists
# on Slave which implies that 'CREATE DATABASE db1' is filtered
# out.
#
USE db1;
ERROR 42000: Unknown database 'db1'
#
# Execute other database commands (ALTER/DROP) on Master.
#
[connection master]
ALTER DATABASE db1 CHARACTER SET latin1;
DROP DATABASE db1;
#
# Check that we are able to sync with slave successfully
# which implies that those commands are filtered out.
# If they were executed by Slave, that will break replication
# as we do not have 'db1' database on Slave.
include/sync_slave_sql_with_master.inc
#
# Cleanup
#
include/rpl_end.inc
