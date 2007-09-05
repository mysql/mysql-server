# Replication tests need binlog
source include/have_log_bin.inc;

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT,);

connection slave;
# Check that server is compiled and started with support for NDB
disable_query_log;
--require r/true.require
select (support = 'YES' or support = 'DEFAULT') as `TRUE` from information_schema.engines where engine = 'ndbcluster';
--source include/ndb_not_readonly.inc
enable_query_log;

-- source include/master-slave-reset.inc

# Set the default connection to 'master'
connection master;
