#
# Wait for the metadata sync blacklist to become empty
#
# Arguments:
#  max_wait - number of seconds to wait before timing out
#
--let $blacklist_size = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_blacklist_size', Value, 1)
expr $max_wait_intervals = $max_wait + $max_wait;
while ($blacklist_size != 0)
{
  --sleep 0.5
  --dec $max_wait_intervals
  if ($max_wait_intervals == 0)
  {
    --echo Blacklist size = $blacklist_size when it should be 0
    --die Timed out waiting for blacklist entries to be removed
  }
  --let $blacklist_size = query_get_value(SHOW STATUS LIKE 'Ndb_metadata_blacklist_size', Value, 1)
}
