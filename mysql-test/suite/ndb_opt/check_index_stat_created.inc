#
# Check if index stats have been created for a table
#
# Usage:
#
# let $index_stat_db=<db name>;
# let $index_stat_table=<table name>;
# source check_index_stats_created.inc;
#

disable_query_log;
if (`SELECT LENGTH('$index_stat_db') = 0`)
{
  # Default is test
  let $index_stat_db=test;
}
if (`SELECT LENGTH('$index_stat_table') = 0`)
{
  # Default is t1
  let $index_stat_table=t1;
}

let $root_name = '$index_stat_db/def/$index_stat_table';
# Get the id of an ordered index on the table
let $index_id =
  query_get_value(SELECT id FROM ndbinfo.dict_obj_tree
                  WHERE root_name = $root_name AND type = 6, id, 1);

# Check the ndbinfo table to see if index stats have been created
eval SELECT CASE COUNT(*)
              WHEN 0 THEN 'NO'
              ELSE 'YES'
            END AS stats_created
     FROM ndbinfo.index_stats
     WHERE index_id = $index_id;
enable_query_log;

# Clean up
let $index_stat_db=;
let $index_stat_table=;
