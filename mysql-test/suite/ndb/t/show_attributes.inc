#
# show_attributes.inc - print only attribute details from NDB_DESC
#
# Usage:
# [let $show_attributes_db=<db name>;]
# [let $show_attributes_table=<table name>;]
# --source show_attributes.inc
#

--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/desc.txt;

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


# Dump the output of ndb_desc to file
--exec $NDB_DESC --no-defaults -d $show_attributes_db $show_attributes_table > $dump_file

# Load the file into a temporary table
CREATE TEMPORARY TABLE test.desc (a varchar(512));

--replace_result $dump_file DUMP_FILE
eval LOAD DATA INFILE '$dump_file' INTO TABLE test.desc;
--remove_file $dump_file

# Query the temporary table
--replace_regex /BT=NDB\$BLOB_[0-9]+_[0-9]+/BT=NDB$BLOB_#_#/
SELECT TRIM(TRAILING "\r" FROM a) as 'Attributes:' FROM test.desc
  WHERE a LIKE "%ST=%";

DROP TABLE test.desc;
let $show_attributes_db=;
let $show_attributes_table=;
--enable_query_log
