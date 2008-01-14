# Setup connections to both MySQL Servers connected to the cluster
# a bit different if in embedded and not

# Check that server has NDB support
--source include/have_ndb.inc

-- let have_embedded=`select version() like '%embedded%'`
if ($have_embedded)
{
  connect (server1,localhost,root,,test);
  connect (server2,localhost,root,,test);
}
if (!$have_embedded)
{
  connect (server1,127.0.0.1,root,,test,$MASTER_MYPORT,);
  connect (server2,127.0.0.1,root,,test,$MASTER_MYPORT1,);
}

# Check that server2 has NDB support
connection server2;
--source include/have_ndb.inc

# Set the default connection
connection server1;
