#
# When developing new tests, consider using a specific SELECT from ndbinfo
# tables (including the wl#11968 tables: dictionary_tables, dictionary_columns,
# blobs, index_columns, foreign_keys, events) to get brief and clear results.
#
# This file is meant to replace some uses of ndb_desc_print.inc with faster
# and more compact queries of ndbinfo tables.
#
# Set $ndb_describe_table to table name
# Set $ndb_describe_db to database name
#
# With $ndb_describe_indexes=1, include index info (like ndb_desc -i)
# With $ndb_describe_blobs=1, include blob info (like ndb_desc -b)
#
# The basic table description is almost perfectly backward-compatible with
# ndb_desc. The index and blob descriptions, however, differ substantial ways,
# since "ndb_desc -i" prints details of hidden index tables that are not
# generally available in NDB API.
#
# "ndb_desc -p" is not emulated at all here. It computes data distribution
# in a way that is not yet available anywhere else.

assert($ndb_describe_table);
if(! $ndb_describe_db)
{
  let $ndb_describe_db=test;
}

disable_query_log ONCE;
disable_result_log ONCE;
eval SELECT table_id into @describe_table_id from ndbinfo.dictionary_tables
     WHERE  table_name = "$ndb_describe_table"
     AND database_name = "$ndb_describe_db";

disable_query_log ONCE;
eval SELECT CONCAT(
  "Version: Any",
  "\nFragment type: ", fragment_type,
  "\nK Value: 6",
  "\nMin load factor: 78",
  "\nMax load factor: 80",
  "\nTemporary table: ", IF(logging, "no", "yes"),
  "\nNumber of attributes: ", attributes,
  "\nNumber of primary keys: ", primary_key_cols,
  "\nLength of frm data: XXX",
  "\nMax Rows: ", max_rows,
  "\nRow Checksum: ", checksum,
  "\nRow GCI: ", contains_GCI,
  "\nSingleUserMode: ", single_user_mode-1,
  "\nForceVarPart: ", force_var_part,
  "\nPartitionCount: ", partitions,
  "\nFragmentCount: ", fragments,
  "\nPartitionBalance: ", partition_balance,
  "\nExtraRowGciBits: ", GCI_bits,
  "\nExtraRowAuthorBits: ", author_bits,
  "\nTableStatus: ", status,
  "\nTable options:", IF(read_backup, " readbackup", ""),
                      IF(read_backup AND fully_replicated, ", ", ""),
                      IF(fully_replicated, "fullyreplicated", ""),
  IF(CHAR_LENGTH(hash_map), CONCAT("\nHashMap: ", hash_map), "")
  ) AS "-- $ndb_describe_table --"
  FROM ndbinfo.dictionary_tables
  WHERE table_id = @describe_table_id;

let $tablespace=`SELECT tablespace FROM ndbinfo.dictionary_tables
     WHERE table_id = @describe_table_id`;
if($tablespace)
{
  disable_query_log ONCE;
  eval SELECT CONCAT("Tablespace: ", fq_name) AS "Tablespace id: XX"
  FROM ndbinfo.dict_obj_info where id=$tablespace and type=20;
}

disable_query_log ONCE;
replace_regex /NDB\$BLOB_[0-9]*_/NDB$BLOB_XX_/;
SELECT CONCAT(
  name, " ", column_type, " ",
  IF(c.primary_key, "PRIMARY KEY", nullable),
  IF(c.partition_key, " DISTRIBUTION KEY", ""),
  " AT=", array_type,
  " ST=", storage_type,
  IF(c.auto_inc, " AUTO_INCR", ""),
  IF(ISNULL(blob_table_name), "", concat(" BV=2 BT=", blob_table_name)),
  IF(c.dynamic, " DYNAMIC", ""),
  IF(CHAR_LENGTH(c.default_value), CONCAT(" DEFAULT ", c.default_value), "")
)
  AS "-- Attributes --"
  FROM ndbinfo.dictionary_columns c
  JOIN ndbinfo.dictionary_tables t USING (table_id)
  LEFT OUTER JOIN ndbinfo.blobs b USING (table_id, column_id)
  WHERE table_id = @describe_table_id;

echo -- Indexes -- ;

disable_query_log ONCE;
disable_result_log ONCE;
SELECT
  CONCAT("PRIMARY KEY(", REPLACE(primary_key,",",", "), ") - UniqueHashIndex")
  INTO @pkdescriptor FROM ndbinfo.dictionary_tables
  WHERE table_id = @describe_table_id;
let $pk_descriptor=`SELECT @pkdescriptor`;

disable_query_log ONCE;
EVAL SELECT
  CONCAT(index_name,
         "(", REPLACE(columns,",",", "), ") - ",
         REPLACE(REPLACE(type_name," hash","Hash")," index","Index"))
    AS "$pk_descriptor"
  FROM ndbinfo.index_columns
  JOIN ndbinfo.dict_obj_types ON index_type = type_id
  WHERE table_id = @describe_table_id
  ORDER BY index_object_id;

if($ndb_describe_indexes)
{
  disable_query_log ONCE;
  SELECT
    CONCAT("-- ", table_name, "/", index_name, " --\n",
           "Attributes: ", columns, "\n",
           "Index Type: ", type_name, "\n",
           "Index Status: ", status, "\n",
           "Partitions: ", tab_partitions, "\n",
           "Fragments: ", tab_fragments) AS ""
    FROM ndbinfo.index_columns t1
    JOIN ndbinfo.dict_obj_types ON t1.index_type = type_id
    JOIN ndbinfo.table_distribution_status t3 ON index_object_id = t3.table_id
    WHERE t1.table_id = @describe_table_id
    ORDER BY index_object_id;
}

if($ndb_describe_blobs)
{
  echo;
  disable_query_log ONCE;
  replace_regex /NDB\$BLOB_[0-9]*_/NDB$BLOB_XX_/;
  SELECT CONCAT(
  "Column name: ", column_name,
  "\nInline size: ", inline_size,
  "\nPart size: ", part_size,
  "\nStipe size: ", stripe_size,
  "\nBlob Table Name: ", blob_table_name,
  "\nFragment type: ", fragment_type,
  "\nTemporary table: ", IF(logging, "no", "yes"),
  "\nNumber of attributes: ", attributes,
  "\nPrimary key: ", primary_key,
  "\nMax Rows: ", max_rows,
  "\nRow Checksum: ", checksum,
  "\nRow GCI: ", contains_GCI,
  "\nSingleUserMode: ", single_user_mode-1,
  "\nDynamic: ", dynamic,
  "\nPartitionCount: ", partitions,
  "\nFragmentCount: ", fragments,
  "\nPartitionBalance: ", partition_balance,
  "\nExtraRowGciBits: ", GCI_bits,
  "\nExtraRowAuthorBits: ", author_bits,
  "\nTableStatus: ", status,
  "\nTable options:", IF(read_backup, " readbackup", ""),
                      IF(read_backup AND fully_replicated, ", ", ""),
                      IF(fully_replicated, "fullyreplicated", ""),
  "\nHashMap: ", hash_map,
  "\n") AS "-- Blob Tables --"
  FROM ndbinfo.dictionary_tables t1
  JOIN ndbinfo.blobs on t1.table_name = blob_table_name
  WHERE blobs.table_id = @describe_table_id;
}

# Formatting for perfect compatibility with old result files:
if ($ndb_describe_print_exit)
{
  dec $ndb_describe_print_exit;
  while($ndb_describe_print_exit)
  {
    echo;
    dec $ndb_describe_print_exit;
  }
  echo NDBT_ProgramExit: 0 - OK;
  echo;
}


# Reset argument variable in order to detect missing assignment
let $ndb_describe_table=;
let $ndb_describe_db=;
let $ndb_describe_indexes=0;
let $ndb_describe_blobs=0;
