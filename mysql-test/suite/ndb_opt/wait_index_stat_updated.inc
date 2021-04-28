#
# Wait until index stats have been updated for a table.
# Times out after waiting for 30 seconds
#
# Usage:
#
# --let $index_stat_db=<db name>
# --let $index_stat_table=<table name>
# --source wait_index_stats_updated.inc
#

--disable_query_log
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

--let $root_name = '$index_stat_db/def/$index_stat_table'
# Get the id of an ordered index on the table
--let $index_id = query_get_value(SELECT id FROM ndbinfo.dict_obj_tree WHERE root_name = $root_name AND type = 6, id, 1)

CREATE TEMPORARY TABLE tmp_table (
    x char(255)
  ) CHARSET latin1 ENGINE InnoDB;

CREATE TEMPORARY TABLE index_stats (
  index_id INT UNSIGNED,
  index_version INT UNSIGNED,
  table_id INT UNSIGNED,
  frag_count INT UNSIGNED,
  value_format INT UNSIGNED,
  sample_version INT UNSIGNED,
  load_time INT UNSIGNED,
  sample_count INT UNSIGNED,
  key_bytes INT UNSIGNED
) ENGINE InnoDB;
--enable_query_log

--let $sample_version = 1
# Set max intervals to 6 i.e. 6 waits of 5 seconds each
--let $max_wait_intervals = 6
while ($sample_version < 2)
{
  --sleep 5
  --dec $max_wait_intervals
  if ($max_wait_intervals == 0)
  {
    --die Timed out waiting for index stats to be updated
  }
  --disable_query_log
  # Retrieve the contents of the ndb_index_stat_head table
  --let $dump_file1 = $MYSQLTEST_VARDIR/tmp/index_stat_select_all1.txt
  --exec $NDB_SELECT_ALL -d mysql --delimiter=, ndb_index_stat_head > $dump_file1
  --eval LOAD DATA INFILE '$dump_file1' INTO TABLE tmp_table

  # Remove the column names
  DELETE FROM tmp_table WHERE x NOT REGEXP '^[0-9]*,';

  # Dump the contents so that it can be loaded into another table which
  # can be queried
  --let $dump_file2 = $MYSQLTEST_VARDIR/tmp/index_stat_select_all2.txt
  --eval SELECT x FROM tmp_table INTO OUTFILE '$dump_file2'
  --eval LOAD DATA INFILE '$dump_file2' INTO TABLE index_stats FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n'

  # Filter out content about indexes on other tables
  --eval DELETE FROM index_stats WHERE index_id <> $index_id

  # Update sample_version with value retrieved from ndb_index_stat_head
  --let $sample_version = query_get_value(SELECT sample_version FROM index_stats, sample_version, 1)

  # Clean up
  --remove_file $dump_file1
  --remove_file $dump_file2
  TRUNCATE TABLE tmp_table;
  TRUNCATE TABLE index_stats;
  --enable_query_log
}

--echo Stats automatically updated
#--echo Sample version: $sample_version

--disable_query_log ONCE
DROP TABLE tmp_table,index_stats;
--let $index_stat_db=
--let $index_stat_table=
