#
# Wait until index stats have been updated for a table.
# Times out after waiting for 30 seconds
#
# Usage:
#
# let $index_stat_db=<db name>;
# let $index_stat_table=<table name>;
# source wait_index_stats_updated.inc;
#

if (`select LENGTH('$index_stat_db') = 0`)
{
  # Default is test
  let $index_stat_db=test;
}
if (`select LENGTH('$index_stat_table') = 0`)
{
  # Default is t1
  let $index_stat_table=t1;
}

let $root_name = '$index_stat_db/def/$index_stat_table';
# Get the id of an ordered index on the table
let $index_id =
  query_get_value(SELECT id FROM ndbinfo.dict_obj_tree
                  WHERE root_name = $root_name AND type = 6, id, 1);

# Wait until a new sample version shows up in the ndbinfo table
let $wait_condition = SELECT MAX(sample_version) > 1
                      FROM ndbinfo.index_stats
                      WHERE index_id = $index_id;
source include/wait_condition.inc;

echo Stats automatically updated;
#echo Sample version: $sample_version;

let $index_stat_db=;
let $index_stat_table=;
