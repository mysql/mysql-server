#
# Setup and check that there are two MySQL Servers with
# support for ndbcluster available
#

# Check that required variables are defined(these are normally set in the
# suite or test specific my.cnf file)
if (!$MASTER_MYPORT)
{
  die Need MASTER_MYPORT variable;
}
if (!$MASTER_MYPORT1)
{
  die Need MASTER_MYPORT1 variable;
}

# Connect the to the two MySQL Servers defined in config
connect (server1,127.0.0.1,root,,test,$MASTER_MYPORT,);
connect (server2,127.0.0.1,root,,test,$MASTER_MYPORT1,);

# Check that server1 has NDB support
connection server1;
--source include/have_ndb.inc

# Check that server2 has NDB support
connection server2;
--source include/have_ndb.inc

# Set default connection
connection server1;
