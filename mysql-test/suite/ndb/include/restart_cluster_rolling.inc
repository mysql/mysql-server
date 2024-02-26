##
## Restarts a NDB cluster by rolling node restart
##
## Parameters:
##   $waiter_timeout_start      - How long wo tait for a single node to start, default from restart_node.inc
##   $waiter_timeout_stop       - How long to wait for a single node to stop, default from restart_node.inc
##   $MTR_NDB_FIRST_NDBD_NODEID - The first node id of the NDBD data nodes
##   $MTR_NDB_LAST_NDBD_NODEID  - The last node id of the NDBD data nodes
##
## This include file has as a prerequisite that the NDBD node ids is in a numbered range without
## holes which starts with MTR_NDB_FIRST_NDBD_NODEID and ends with $MTR_NDB_LAST_NDBD_NODEID.
##
## The last two parameters are normally provided by
##   --source suite/ndb/include/ndb_info.inc
##

--echo # Rolling system restart from node $MTR_NDB_FIRST_NDBD_NODEID to node $MTR_NDB_LAST_NDBD_NODEID:
--append_file $NDB_TOOLS_OUTPUT
# Rolling system restart:
EOF

--let $ndb_node_id= $MTR_NDB_LAST_NDBD_NODEID
while (`SELECT $ndb_node_id >= $MTR_NDB_FIRST_NDBD_NODEID`) {
  --echo # Restarting node $ndb_node_id as part of rolling system restart
  --source suite/ndb/include/restart_node.inc
  --dec $ndb_node_id
}
--let $ndb_node_id=
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
