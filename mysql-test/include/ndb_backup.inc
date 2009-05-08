######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################

# Bug#41307: Tests using include/ndb_backup.inc won't work on Windows due to
#            'grep' call
# This test is disabled on Windows via the next line until the above bug is
# resolved
--source include/not_windows.inc

--disable_query_log
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "start backup" >> $NDB_TOOLS_OUTPUT

--source include/ndb_backup_id.inc
--enable_query_log

