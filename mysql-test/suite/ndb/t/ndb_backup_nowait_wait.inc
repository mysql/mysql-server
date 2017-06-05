# Want to start a backup, but not wait for
# it to finish
# Use NdbInfo to tell when backup has finished.
--echo Waiting for backup to complete
let $_done = 0;
let $_completed_timeout = 60;
while(!$_done)
{
  let $_backup_records_in_use = `SELECT SUM(used) from ndbinfo.ndb\\\$pools where block_number=244 and pool_name = "Backup Record"`;

  if ($_backup_records_in_use == $start_backup_records_in_use)
  {
    let $_done = 1;
  }

  if (!$_done)
  {
    dec $_completed_timeout;
    #--echo Waiting for backup to start
    sleep 1;
    if (!$_completed_timeout)
    {
      let $_done = 1;
    }
  }
}

if (!$_completed_timeout)
{
  die "Timed out waiting for backup to complete.";
}

# Obtain current backup id...

--disable_query_log
--source include/ndb_backup_id.inc
--enable_query_log

--echo Backup completed
