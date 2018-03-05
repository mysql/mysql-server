# This file is paired with force_cats.inc.
# It takes the pressure off InnoDB, so it has a chance to
# fallback from CATS to FCFS.
--disable_query_log
--disable_result_log
--let $cats_connection_number = 0
while($cats_connection_number <= $LOCK_CATS_THRESHOLD)
{
  --let $cats_connection_name = cats$cats_connection_number
  --connection $cats_connection_name
  --echo # cleaning up connection $cats_connection_name
  if($cats_connection_number > 0){
    --reap
  }
  ROLLBACK;
  --disconnect $cats_connection_name

  --inc $cats_connection_number
}
--connection default
DROP TABLE cats;
--enable_result_log
--enable_query_log
