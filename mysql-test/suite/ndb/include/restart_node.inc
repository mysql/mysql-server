##
## Restarts a single NDB data node
##
## Parameters:
##   $ndb_node_id          - The node to be restarted
##   $waiter_timeout_start - How long wo tait for a single node to start, default 120 seconds
##   $waiter_timeout_stop  - How long to wait for a single node to stop, default 60 seconds
##
## In addition, $NDB_CONNECTSTRING, $NDB_MGM, $NDB_TOOLS_DIR and $NDB_TOOLS_OUTPUT
## must be set (they are normally proivided by MTR).
##

# Default values
if (`SELECT LENGTH('$waiter_timeout_start') = 0`) {
  let $waiter_timeout_start= 120;
}
if (`SELECT LENGTH('$waiter_timeout_stop') = 0`) {
  let $waiter_timeout_stop= 60;
}

--echo # Restarting node $ndb_node_id, timeouts: start: $waiter_timeout_start seconds, stop: $waiter_timeout_stop seconds:

--append_file $NDB_TOOLS_OUTPUT
# Restarting random node, stopping part:
EOF
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "$ndb_node_id restart -n" >> $NDB_TOOLS_OUTPUT
--exec $NDB_TOOLS_DIR/ndb_waiter --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" --wait-nodes=$ndb_node_id --not-started --timeout=$waiter_timeout_stop >> $NDB_TOOLS_OUTPUT

--append_file $NDB_TOOLS_OUTPUT
# Restarting random node, starting part:
EOF
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "$ndb_node_id start" >> $NDB_TOOLS_OUTPUT
--exec $NDB_TOOLS_DIR/ndb_waiter --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" --timeout=$waiter_timeout_start >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
