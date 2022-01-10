#
# show_varpart.inc - print only varpart details from NDB_DESC
#
# Usage:
# let $show_varpart_db=<db name>;
# let $show_varpart_table=<table name>;
# --source show_varpart.inc
#

--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/desc.txt;

if (`select LENGTH('$show_varpart_db') = 0`)
{
  # Default is to show properties of test
  let $show_varpart_db=test;
}

if (`select LENGTH('$show_varpart_table') = 0`)
{
  # Default is to show properties of t1
  let $show_varpart_table=t1;
}

let $output=`SELECT Concat("ForceVarPart: ", force_var_part)
  FROM ndbinfo.dictionary_tables
  WHERE database_name="$show_varpart_db" AND table_name="$show_varpart_table"`;

echo $output;

let $show_varpart_db=;
let $show_varpart_table=;
--enable_query_log
