#
# show_attributes.inc - print table attribute details
# (output format is compatible with ndb_desc)
#
# Usage:
# [let $show_attributes_db=<db name>;]
# [let $show_attributes_table=<table name>;]
# --source show_attributes.inc
#

--disable_query_log

if (`select LENGTH('$show_attributes_db') = 0`)
{
  # Default is to show properties of test
  let $show_attributes_db=test;
}

if (`select LENGTH('$show_attributes_table') = 0`)
{
  # Default is to show properties of t1
  let $show_attributes_table=t1;
}

--replace_regex /BT=NDB\$BLOB_[0-9]+_[0-9]+/BT=NDB$BLOB_#_#/
eval SELECT CONCAT(
  name, " ", column_type, " ",
  IF(c.primary_key, "PRIMARY KEY", nullable),
  IF(c.partition_key, " DISTRIBUTION KEY", ""),
  " AT=", array_type,
  " ST=", storage_type,
  IF(c.auto_inc, " AUTO_INCR", ""),
  IF(ISNULL(blob_table_name), "", CONCAT(" BV=2 BT=", blob_table_name)),
  IF(c.dynamic, " DYNAMIC", ""),
  IF(CHAR_LENGTH(c.default_value), CONCAT(" DEFAULT ", c.default_value), "")
)
AS "Attributes:"
FROM ndbinfo.dictionary_columns c
JOIN ndbinfo.dictionary_tables t USING (table_id)
LEFT OUTER JOIN ndbinfo.blobs b USING (table_id, column_id)
WHERE t.table_name = "$show_attributes_table"
AND t.database_name = "$show_attributes_db";

let $show_attributes_db=;
let $show_attributes_table=;
--enable_query_log
