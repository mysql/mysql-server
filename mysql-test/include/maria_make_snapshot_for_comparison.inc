# Maria helper script
# Copies clean tables' data and index file to other directory
# Tables are t1...t[$mms_tables]
# They are later used as a reference to see if recovery works.

# API:
# set $mms_tables to N, the script will cover tables mysqltest.t1,...tN

connection admin;

let $mms_table_to_use=$mms_tables;
let $mms_purpose=comparison;
let $mms_copy=1;

--disable_query_log
--disable_warnings
eval drop database if exists mysqltest_for_$mms_purpose;
--enable_warnings
eval create database mysqltest_for_$mms_purpose;
--enable_query_log

while ($mms_table_to_use)
{
  # to serve as a reference, table must be in a clean state
  eval flush table t$mms_table_to_use;
  -- source include/maria_make_snapshot.inc
  dec $mms_table_to_use;
}
let $mms_copy=0;
connection default;
