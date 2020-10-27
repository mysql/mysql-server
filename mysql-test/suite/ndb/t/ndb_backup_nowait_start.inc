# Want to start a backup, but not wait for
# it to finish
# We use NdbInfo to tell when it has started...
--disable_query_log
--source include/ndb_backup_id.inc
--enable_query_log
let $_pre_backup_id = $the_backup_id;
let $start_backup_records_in_use = `SELECT SUM(used) from ndbinfo.ndb\\\$pools where block_number=244 and pool_name = "Backup Record"`;

--echo Starting backup
# Use WAIT STARTED rather than NOWAIT
# NOWAIT does not wait for a BACKUP_CONF in MGMD, and so does not handle e.g.
# the case where MGMD sends the BACKUP_REQ to the wrong Master.
# This can result in no backup being started, silently, resulting in test case
# failures
#
--let $START_BACKUP_OUTFILE=$MYSQLTEST_VARDIR/tmp/start_backup_outfile.txt
--exec $NDB_MGM -e "start backup $start_backup_extra wait started" >> $START_BACKUP_OUTFILE

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
  --echo Timed out waiting for backup to start
  --echo Pre backup id $_pre_backup_id
  --echo The backup id $the_backup_id
  --echo Start backup records in use $start_backup_records_in_use
  --echo Backup records in use $_backup_records_in_use
  --echo Start Backup Extra $start_backup_extra.
  --cat_file $START_BACKUP_OUTFILE
  die "Timed out waiting for backup to start.";
}

--remove_file $START_BACKUP_OUTFILE
--echo Backup started
