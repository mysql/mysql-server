#
# ndb_check_blob_tables.inc - check that all blob tables use same hashmap as main table
#
# Usage:
# let $ndb_die_on_error=1; # 1 if it should fail on hashmap mismatch
# let $ndb_database=<table database>;
# let $ndb_table=<table name>;
# --source suite/ndb/include/ndb_check_blob_tables.inc
#

let $header=`SELECT CONCAT("Table: ", "$ndb_database", ".", "$ndb_table")`;

disable_query_log ONCE;
eval SELECT table_id into @describe_table_id from ndbinfo.dictionary_tables
  WHERE  table_name = "$ndb_table" AND database_name = "$ndb_database";

let $hashmap=`SELECT hash_map FROM ndbinfo.dictionary_tables WHERE table_id=@describe_table_id`;

echo $header;
echo HashMap: $hashmap;

disable_query_log ONCE;
SELECT CONCAT("HashMap: ", hash_map, "\n") AS ""
  FROM ndbinfo.dictionary_tables t JOIN ndbinfo.blobs b
  ON t.table_name = b.blob_table_name
  WHERE b.table_id = @describe_table_id;

if($ndb_die_on_error)
{
  disable_query_log ONCE;
  LET $unmatched=`SELECT count(*)
    FROM ndbinfo.dictionary_tables t JOIN ndbinfo.blobs b
    ON t.table_name = b.blob_table_name
    WHERE b.table_id=@describe_table_id AND t.hash_map != "$hashmap"`;

  assert($unmatched == 0);
}

let $hashmap=;
let $ndb_die_on_error=;
let $ndb_database=;
let $ndb_table=;
