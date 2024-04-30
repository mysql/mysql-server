#
# Detect and sync all metadata mismatches between NDB Dictionary and DD
#
# Arguments:
#  max_wait - number of seconds to wait before timing out
#

# Trigger immediate synchronization
SET GLOBAL ndb_metadata_sync = true;

# Wait for synchronization to complete
let $metadata_sync_value = `SELECT @@ndb_metadata_sync`;
expr $max_wait_intervals = $max_wait + $max_wait;
while ($metadata_sync_value != 0)
{
  sleep 0.5;
  dec $max_wait_intervals;
  if ($max_wait_intervals == 0)
  {
    die Timed out waiting for metadata changes to be synced;
  }
  let $metadata_sync_value = `SELECT @@ndb_metadata_sync`;
}
