##
## Restarts a single random NDB data node
##
## Parameters:
##   $waiter_timeout_start      - How long wo tait for a single node to start, default from restart_node.inc
##   $waiter_timeout_stop       - How long to wait for a single node to stop, default from restart_node.inc
##   $MTR_NDB_FIRST_NDBD_NODEID - The first node id of the NDBD data nodes
##   $MTR_NDB_NO_OF_NODES       - The number of NDBD data nodes
##
## This include file has as a prerequisite that the NDBD node ids is in a numbered range without
## holes which starts with MTR_NDB_FIRST_NDBD_NODEID and has no more than MTR_NDB_NO_OF_NODES nodes.
##
## The last two parameters are normally provided by
##   --source suite/ndb/include/ndb_info.inc
##
## In addition, $NDB_CONNECTSTRING, $NDB_MGM, $NDB_TOOLS_DIR and $NDB_TOOLS_OUTPUT
## must be set (they are normally proivided by MTR).
##

--let $ndb_node_id = `SELECT FLOOR($MTR_NDB_FIRST_NDBD_NODEID + (RAND() * $MTR_NDB_NO_OF_NODES))`
--echo # Restarting random node $ndb_node_id
--source suite/ndb/include/restart_node.inc
--let $ndb_node_id=
