# ==== Purpose ====
#
# Set up circular cluster replication where each 
# cluster has two mysqlds and replication directions are 
# following:
#          master ---> slave  
#           /            \
#     cluster A        cluster B
#           \            /
#         master1 <--- slave1
#
# ==== Usage ====
#
#   [--let $rpl_server_count= N]
#   [--let $rpl_skip_check_server_ids= 1]
#   [--let $rpl_skip_reset_master_and_slave= 1]
#   [--let $rpl_skip_change_master= 1]
#   [--let $rpl_skip_start_slave= 1]
#   [--let $rpl_debug= 1]
#   [--let $slave_timeout= NUMBER]
#   --source include/ndb_master-slave_2ch.inc
#
# Parameters:
#   $rpl_server_count, $rpl_skip_check_server_ids,
#   $rpl_skip_reset_master_and_slave, $rpl_skip_change_master,
#   $rpl_skip_start_slave, $rpl_debug, $slave_timeout
#     See include/master-slave.inc

--let $rpl_topology= 1->2,4->3
--let $rpl_skip_check_server_ids= 1
--source include/rpl_init.inc

# Make connections to mysqlds

--let $rpl_connection_name= master
--let $rpl_server_number= 1
--source include/rpl_connect.inc

--let $rpl_connection_name= master1
--let $rpl_server_number= 1
--source include/rpl_connect.inc

--let $rpl_connection_name= slave
--let $rpl_server_number= 2
--source include/rpl_connect.inc

--let $rpl_connection_name= slave1
--let $rpl_server_number= 2
--source include/rpl_connect.inc


# Check that all mysqld are compiled with ndb support
--let $_rpl_server= 4
while ($_rpl_server)
{
  --connection server_$_rpl_server
  if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.ENGINES WHERE engine = 'ndbcluster' AND (support = 'YES' OR support = 'DEFAULT')`)
  {
    --skip Test requires NDB.
  }
  --source include/ndb_not_readonly.inc
  --dec $_rpl_server
}


# Set the default connection to 'master' (cluster A)
connection master;
