##
## Restarts a NDB cluster
##
## Parameters:
##   $waiter_timeout_start      - How long wo tait for a cluster to start, default 600 seconds
##   $waiter_timeout_stop       - How long to wait for a cluster to stop, default 30 seconds
##

# Default values
if (`SELECT LENGTH('$waiter_timeout_start') = 0`) {
  let $waiter_timeout_start= 600;
}
if (`SELECT LENGTH('$waiter_timeout_stop') = 0`) {
  let $waiter_timeout_stop= 30;
}

--echo # System restart:
--append_file $NDB_TOOLS_OUTPUT
# System restart:
EOF

--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "all restart -n" >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
--exec $NDB_WAITER --not-started --timeout=$waiter_timeout_stop >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "all start" >> $NDB_TOOLS_OUTPUT
--exec $NDB_WAITER --timeout=$waiter_timeout_start >> $NDB_TOOLS_OUTPUT
--exec $NDB_MGM -e "show" >> $NDB_TOOLS_OUTPUT
