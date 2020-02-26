--disable_query_log
--disable_result_log
SET @prev_metadata_check = @@global.ndb_metadata_check;
SET GLOBAL ndb_metadata_check=ON;
SET GLOBAL ndb_metadata_sync=ON;
--let $metadata_sync_running=1

--echo Waiting for metadata sync
while ($metadata_sync_running)
{
  --let $metadata_sync_running=query_get_value(select @@global.ndb_metadata_sync as m, m, 1)
  --sleep 0.1
}

--echo Metadata synchronised
SET GLOBAL ndb_metadata_check=@prev_metadata_check;

--enable_result_log
--enable_query_log