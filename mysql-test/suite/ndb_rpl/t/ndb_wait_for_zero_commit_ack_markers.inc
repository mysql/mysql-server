let $wait_counter=30;
let $wait_timeout=1;

while ($wait_counter)
{
  # Sum used across all ndb$pools rows with 'CommitAckMarker' type 
  let $success= query_get_value("SELECT SUM(used) = 0 AS result FROM ndbinfo.ndb\\\$pools where pool_name LIKE 'Commit Ack Marker'", result, 1);

  if ($success)
  {
    let $wait_counter=0;
  }

  if (!$success)
  {
    sleep $wait_timeout;
    dec $wait_counter;
  }
}

if (!$success)
{
  SELECT * FROM ndbinfo.ndb$pools where pool_name LIKE 'Commit Ack Marker';
  --die Failed waiting for CommitAckMarker use to go to zero.
}