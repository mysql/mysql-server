#
# show_partition_info.inc - print only partition info
#
#
# Usage:
# let ndb_table=<table name>;
# --source show_partition_info.inc
#

if (!$ndb_table)
{
  echo The variable ndb_table must be set when calling show_partition_info.inc;
  die Missing argument ndb_table;
}

disable_query_log ONCE;
eval
SELECT CONCAT("PartitionCount: ", count(distinct(fragment_num))) as 'TABLE_INFO'
  FROM ndbinfo.memory_per_fragment
  WHERE fq_name like "%$ndb_table";

let ndb_table=;
