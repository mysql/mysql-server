######################################################
# By JBM 2006-02-16 So that the code is not repeated #
# in test cases and can be reused.                   #
######################################################

--exec $NDB_RESTORE -p 8 -b $the_backup_id -n 1 -m -r --print --print_meta $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT

--exec $NDB_RESTORE -p 8 -b $the_backup_id -n 2  -r --print --print_meta $NDB_BACKUPS-$the_backup_id >> $NDB_TOOLS_OUTPUT
