--source include/not_embedded.inc
--source have_federated_db.inc

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);

connection master;
--disable_warnings
DROP DATABASE IF EXISTS federated;
--enable_warnings
CREATE DATABASE federated;

connection slave;
--disable_warnings
DROP DATABASE IF EXISTS federated;
--enable_warnings
CREATE DATABASE federated;
