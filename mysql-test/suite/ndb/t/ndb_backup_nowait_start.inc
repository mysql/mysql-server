#
# Start backup, but dont wait for it to complete
# - also save the number of "Backup records" in DBLQH before start, this value
#   is later used to wait for backup to complete in ndb_backup_nowait_wait.inc
#

# Save backup id before backup start
--source include/ndb_backup_id.inc
let $_pre_backup_id = $the_backup_id;

# Save backup records in BACKUP before backup start
let $start_backup_records_in_use =
  `SELECT SUM(used) from ndbinfo.ndb\\\$pools
     WHERE block_number=244 AND pool_name = "Backup Record"`;

# Print start backup message, show any extra parameters
let $_start_backup_msg = Starting backup;
if ($start_backup_extra){
  let $_start_backup_msg = Starting $start_backup_extra backup;
}
echo $_start_backup_msg;

# Use WAIT STARTED rather than NOWAIT
# NOWAIT does not wait for a BACKUP_CONF in MGMD, and so does not handle e.g.
# the case where MGMD sends the BACKUP_REQ to the wrong Master. This could
# result in no backup being started, silently, resulting in test case
# failures
--let $START_BACKUP_OUTFILE=$MYSQLTEST_VARDIR/tmp/start_backup_outfile.txt
--exec $NDB_MGM -e "start backup $start_backup_extra wait started" >> $START_BACKUP_OUTFILE

# Make sure that backup id has increased (this is a sanity check that backup
# has changed state)
#
--source include/ndb_backup_id.inc
if ($the_backup_id <= $_pre_backup_id)
{
  echo Pre backup id: $_pre_backup_id;
  echo The backup id: $the_backup_id;
  echo Start backup extra: "$start_backup_extra";
  echo Start backup records: $start_backup_records_in_use;
  echo === Dumping log from "start backup" ==;
  cat_file $START_BACKUP_OUTFILE;
  echo === =============================== ==;
  die The backup id was not increased during start of backup;
}

--remove_file $START_BACKUP_OUTFILE

# NOTE! When using this include file without stalling the backup with an
# error insert it might even have completed at this point

--echo Backup started

# Reset parameters
let $start_backup_extra =;
