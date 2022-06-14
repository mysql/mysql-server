#
# Wait for backup to complete
# - wait until the number of "Backup records" in BACKUP has reached the same
#   value as before backup was started by ndb_backup_nowait_start.inc
#

# Check that value to wait for has been saved
if (!$start_backup_records_in_use)
{
  die Requires start_backup_records_in_use parameter;
}

--echo Waiting for backup to complete
let $_done = 0;
let $_completed_timeout = 60;
while(!$_done)
{
  let $_backup_records_in_use =
    `SELECT SUM(used) from ndbinfo.ndb\\\$pools
      WHERE block_number=244 AND pool_name = "Backup Record"`;

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
      die "Timed out waiting for backup to complete.";
    }
  }
}


# Obtain current backup id...
--source include/ndb_backup_id.inc

--echo Backup completed

let $start_backup_records_in_use =;
