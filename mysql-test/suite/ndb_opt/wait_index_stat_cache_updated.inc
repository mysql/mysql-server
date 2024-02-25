#
# Check if the index stat cache has been updated. This is done by keeping
# track of the events on the ndb_index_stat_head table which is exposed by
# the Ndb_index_stat_event_count status variable.
#
# Usage:
#
# --let $old_event_count=
#     query_get_value(SHOW STATUS LIKE 'Ndb_index_stat_event_count', Value, 1)
#  < Changes that are expected to cause an auto-update of index stats >
# --let $index_stat_db=<db name>
# --let $index_stat_table=<table name>
# --source wait_index_stat_cache_updated.inc
#

if (`select LENGTH('$index_stat_db') = 0`)
{
  # Default is test
  --let $index_stat_db=test
}
if (`select LENGTH('$index_stat_table') = 0`)
{
  # Default is t1
  --let $index_stat_table=t1
}

#--echo Old event count = $old_event_count
--let $root_name = $index_stat_db/def/$index_stat_table
# Get index count
--let $index_count = query_get_value(SELECT COUNT(*) FROM ndbinfo.dict_obj_tree WHERE root_name = '$root_name' AND type = 6, COUNT(*), 1)
#--echo Index count = $index_count
--expr $expected_event_count = $old_event_count + $index_count
--echo Waiting for the index stats to be updated
# Suppress the echo when the wait times out. It's likely that an event has
# been handled earlier than expected. We are more concerned about scenarios
# where the stats haven't been updated. This will be detected in the select
# queries that follow the wait
--let $silent_failure=1
--let $wait_condition = SELECT VARIABLE_VALUE >= $expected_event_count FROM performance_schema.global_status WHERE VARIABLE_NAME LIKE 'Ndb_index_stat_event_count'
--source include/wait_condition.inc
--echo Index stats updated
