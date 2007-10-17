--source include/have_log_bin.inc
--source include/not_embedded.inc
--source ./include/have_federated_db.inc

source ./include/master-slave.inc;

# remote table creation

# We have to sync with master, to ensure slave had time to start properly
# before we stop it. If not, we get errors about UNIX_TIMESTAMP() in the log.
connection master;
sync_slave_with_master;

connection slave;
#--replicate-ignore-db=federated
stop slave;

--disable_warnings
# at this point, we are connected to master
DROP DATABASE IF EXISTS federated;
--enable_warnings
CREATE DATABASE federated;

connection master;
--disable_warnings
DROP DATABASE IF EXISTS federated;
--enable_warnings
CREATE DATABASE federated;
