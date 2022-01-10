#
# ndb_check_unique_index.inc - check that all unique index use same fragment
# count as main table
#
# Usage:
# let $ndb_database= <table database>;
# let $ndb_table= <table name>;
# --source suite/ndb/include/ndb_check_unique_index.inc
#

disable_query_log ONCE;
disable_result_log ONCE;
eval SELECT table_id into @describe_table_id from ndbinfo.dictionary_tables
     WHERE  table_name = "$ndb_table" AND database_name = "$ndb_database";


let $header=`SELECT concat("Table: ", database_name, ".",table_name)
  FROM ndbinfo.dictionary_tables WHERE table_id = @describe_table_id`;
disable_query_log ONCE;
eval SELECT concat(
    "PartitionCount: ", partitions, "\n",
    "FragmentCount: ", fragments) AS "$header"
  FROM ndbinfo.dictionary_tables WHERE table_id = @describe_table_id;

disable_query_log ONCE;
SELECT CONCAT(
  index_name, "(", columns, ") - UniqueHashIndex\n",
  "FragmentCount: ", tab_fragments) AS ""
  FROM ndbinfo.index_columns t1
  JOIN ndbinfo.table_distribution_status t2 ON t1.index_object_id = t2.table_id
  WHERE index_type = 3 AND t1.table_id = @describe_table_id;

let $ndb_database=;
let $ndb_table=;
