# Replication tests need binlog
source include/have_log_bin.inc;

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT,);

-- source include/master-slave-reset.inc

# Set the default connection to 'master'
connection master;
