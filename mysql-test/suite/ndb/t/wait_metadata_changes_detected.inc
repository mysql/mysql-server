#
# Wait for specified number of metadata changes to be detected
#
# Arguments:
#  initial_detected_count - initial count of changes detected
#  expected_changes - number of changes to wait for
#  max_wait - number of seconds to wait before timing out
#

let $changes_detected = 0;
expr $max_wait_intervals = $max_wait + $max_wait;
while ($changes_detected < $expected_changes)
{
  sleep 0.5;
  dec $max_wait_intervals;
  if ($max_wait_intervals == 0)
  {
    echo $changes_detected changes detected when $expected_changes changes were expected;
    die Timed out waiting for metadata changes to be detected;
  }
  let $current_changes_detected = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_detected_count', Value, 1);
  expr $changes_detected = $current_changes_detected - $initial_detected_count;
}
