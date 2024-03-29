# This inc-file should be called in all test case that does
# "ndb_backup" and "ndb_restore". This inc-file will detele the
# backup directory, created by "ndb_backup". Hence, it should be
# called at the end and MTR will report error if this inc-file is not
# included.

--disable_query_log
if (!$NDB_BACKUP_DIR)
{
  --die Need NDB_BACKUP_DIR variable
}
--let RM_BACKUPS=$NDB_BACKUP_DIR/BACKUP
--enable_query_log
--force-rmdir $RM_BACKUPS
