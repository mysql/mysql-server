# Replication tests need binlog
#
# $skip_replica_start  If true, the slave will not be started
source include/have_log_bin.inc;

connect (master,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (master1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (slave,127.0.0.1,root,,test,$SLAVE_MYPORT,);
connect (slave1,127.0.0.1,root,,test,$SLAVE_MYPORT,);

-- source include/master-slave-reset.inc

source suite/rpl_bhs/extension/bhs/master-slave-bhs.inc;

if (!$skip_replica_start) {
  connection master;
  sync_slave_with_master blackhole_slave;
  sync_slave_with_master;
}


# Set the default connection to 'master'
connection master;
