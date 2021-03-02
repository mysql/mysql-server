######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################
--exec $NDB_MGM -e "start backup" >> $NDB_TOOLS_OUTPUT

--disable_query_log
--source include/ndb_backup_id.inc
--enable_query_log
