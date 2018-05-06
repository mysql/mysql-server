# Want to start a backup, but not wait for
# it to finish
# We use NdbInfo to tell when it has started...
--disable_query_log
--source include/ndb_backup_id.inc
--enable_query_log
let $_pre_backup_id = $the_backup_id;
let $start_backup_records_in_use = `SELECT SUM(used) from ndbinfo.ndb\\\$pools where block_number=244 and pool_name = "Backup Record"`;

--echo Starting backup
--exec $NDB_MGM --no-defaults --ndb-connectstring="$NDB_CONNECTSTRING" -e "start backup nowait" >> $NDB_TOOLS_OUTPUT

let $_done = 0;
let $_start_timeout = 60;
while(!$_done)
{
  let $_backup_records_in_use = `SELECT SUM(used) from ndbinfo.ndb\\\$pools where block_number=244 and pool_name = "Backup Record"`;
  --disable_query_log
  --source include/ndb_backup_id.inc
  --enable_query_log
  
  if ($the_backup_id > $_pre_backup_id)
  {
    # Backup started, maybe even completed
    let $_done = 1;
  }

  if ($_backup_records_in_use > $start_backup_records_in_use)
  {
    let $_done = 1;
  }

  if (!$_done)
  {
    dec $_start_timeout;
    #--echo Waiting for backup to start
    sleep 1;
    if (!$_start_timeout)
    {
      let $_done = 1;
    }
  }
}

if (!$_start_timeout)
{
  die "Timed out waiting for backup to start.";
}

--echo Backup started
