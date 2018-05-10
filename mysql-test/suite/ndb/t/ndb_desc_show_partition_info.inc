#
# ndb_desc_show_partition_info.inc - print only pratirion info details from NDB_DESC
#
# Usage:
# let ndb_table= <table name>;
# --source ndb_desc_show_partition_info.inc
#

if (!$ndb_table)
{
  echo The variable ndb_table must be set when calling ndb_desc_show_partition_info.inc;
  die Missing argument ndb_table;
}
let $table_name= $ndb_table;

--disable_query_log
let $dump_partition = $MYSQLTEST_VARDIR/tmp/partition_info.txt;
--exec $NDB_DESC -dtest $table_name > $dump_partition

# Load the file into a temporary table
CREATE TEMPORARY TABLE partition_info (a varchar(512));

eval LOAD DATA INFILE '$dump_partition' INTO TABLE partition_info
  FIELDS TERMINATED BY '\n';

# Remove Windows line feeds
UPDATE partition_info SET a = TRIM(TRAILING "\r" FROM a);

# Show the two data nodes
SELECT a as 'TABLE_INFO' FROM partition_info
  WHERE a like "PartitionCount%";

DROP TABLE partition_info;
--remove_file $dump_partition
--enable_query_log

let ndb_table=;
