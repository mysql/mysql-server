#
# Wait for specified number of metadata objects to be synced
#
# Arguments:
#  initial_synced_count - initial count of changes synced
#  expected_changes - number of changes to wait for, usually set before
#                     wait_metadata_changes_detected.inc is sourced and
#                     re-used here
#  max_wait - number of seconds to wait before timing out
#
# Note: This include file is usually used after wait_metadata_changes_detected.inc
#
let $changes_synced = 0;
expr $max_wait_intervals = $max_wait + $max_wait;
while ($changes_synced < $expected_changes)
{
  sleep 0.5;
  dec $max_wait_intervals;
  if ($max_wait_intervals == 0)
  {
    echo $changes_synced changes synced when $expected_changes changes synced were expected;
    die Timed out waiting for metadata changes to be synced;
  }
  let $current_changes_synced = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_synced_count', Value, 1);
  expr $changes_synced = $current_changes_synced - $initial_synced_count;
}
