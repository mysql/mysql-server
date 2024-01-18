###############################################################################
# This .inc file is to check that filtering rules on
# CREATE DATABASE/ALTER DATABASE/DROP DATABASE statements
# works properly.
# NOTE: Filter rules should be set in such a way that 'db1'
# is filtered out on Slave. Setting the filtering rule
# should be done in the test that includes this .inc file.
# For eg:
#    --replicate-ignore-db=db1
#    --source ../common/rpl/db_stmts_ignored.inc
#  (OR)
#    --replicate-do-db=db2
#    --source ../common/rpl/db_stmts_ignored.inc
#  (OR)
#    --replicate-wild-ignore-table=db1.%
#    --source ../common/rpl/db_stmts_ignored.inc
###############################################################################
--source include/rpl/connection_source.inc
--echo #
--echo # Execute 'CREATE DATABASE db1' on Master.
--echo #
CREATE DATABASE db1;

--echo #
--echo # Sync with Slave (using gtid values)
--echo #
--let $use_gtids=1
--source include/rpl/sync_to_replica.inc

--echo #
--echo # Check that even after sync is completed 'db1' does not exists
--echo # on Slave which implies that 'CREATE DATABASE db1' is filtered
--echo # out.
--echo #
--error ER_BAD_DB_ERROR
USE db1;

--echo #
--echo # Execute other database commands (ALTER/DROP) on Master.
--echo #
--source include/rpl/connection_source.inc
ALTER DATABASE db1 CHARACTER SET latin1;
DROP DATABASE db1;

--echo #
--echo # Check that we are able to sync with slave successfully
--echo # which implies that those commands are filtered out.
--echo # If they were executed by Slave, that will break replication
--echo # as we do not have 'db1' database on Slave.
--let $use_gtids=1
--source include/rpl/sync_to_replica.inc
