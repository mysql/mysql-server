#
# Wait until there are no objects excluded from metadata sync detection
#
# Arguments:
#  max_wait - number of seconds to wait before timing out
#
--let $excluded_count = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_excluded_count', Value, 1)
expr $max_wait_intervals = $max_wait + $max_wait;
while ($excluded_count != 0)
{
  --sleep 0.5
  --dec $max_wait_intervals
  if ($max_wait_intervals == 0)
  {
    --echo Excluded count = $excluded_count when it should be 0
    --die Timed out waiting for objects to no longer be excluded
  }
  --let $excluded_count = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_excluded_count', Value, 1)
}
