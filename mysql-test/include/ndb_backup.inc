# Start backup
--exec $NDB_MGM -e "start backup" >> $NDB_TOOLS_OUTPUT

# Load current backup id into "the_backup_id" variable
--source include/ndb_backup_id.inc
