#
# show_partition_info.inc - print only partition info
#
# This file previously used ndb_desc; now it queries ndbinfo.dictionary_tables.
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
  SELECT CONCAT("PartitionCount: ", partitions) as 'TABLE_INFO'
  FROM ndbinfo.dictionary_tables
  WHERE table_name="$ndb_table";

let ndb_table=;
