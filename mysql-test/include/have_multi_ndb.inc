# Setup connections to the two different MySQL Servers defined in config
connect (server1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (server2,127.0.0.1,root,,test,$MASTER_MYPORT1,);

# Check that server1 has NDB  support
connection server1;
--source include/have_ndb.inc

# Check that server2 has NDB support
connection server2;
--source include/have_ndb.inc

# Set default connection
connection server1;
