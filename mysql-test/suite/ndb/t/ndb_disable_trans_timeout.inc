# Set a very high transaction timeout value
--exec $NDB_MGM --no-defaults --verbose=0 -e "ALL DUMP 2507 2147483648" >> $NDB_TOOLS_OUTPUT

