# Set a very high transaction timeout value
--exec $NDB_MGM -e "ALL DUMP 2507 2147483648" >> $NDB_TOOLS_OUTPUT

