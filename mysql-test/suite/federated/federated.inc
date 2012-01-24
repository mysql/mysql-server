--source include/not_embedded.inc

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);

connection master;
CREATE DATABASE federated;

connection slave;
CREATE DATABASE federated;
