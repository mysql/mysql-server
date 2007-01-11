--source include/not_embedded.inc
--source ./include/have_federated_db.inc

source ./include/master-slave.inc;

# remote table creation

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
