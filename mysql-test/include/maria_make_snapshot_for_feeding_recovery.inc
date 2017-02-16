# Maria helper script
# Copies tables' data and index file to other directory, and control file.
# Tables are $mms_tname1...$mms_tname[$mms_tables].
# Later, mysqld is shutdown, and that snapshot is put back into the
# datadir, control file too ("flashing recovery's brain"), and recovery is let
# to run on it (see maria_verify_recovery.inc).

# API:
# set $mms_tname to a string, and $mms_tables to a number N, the script will
# cover tables mysqltest.$mms_tname1,...$mms_tnameN


connection admin;
--source include/wait_until_connected_again.inc

let $mms_table_to_use=$mms_tables;
let $mms_purpose=feeding_recovery;
let $mms_copy=1;

--disable_query_log
--disable_warnings
eval drop database if exists mysqltest_for_$mms_purpose;
--enable_warnings
eval create database mysqltest_for_$mms_purpose;
--enable_query_log

while ($mms_table_to_use)
{
  -- source include/maria_make_snapshot.inc
  dec $mms_table_to_use;
}
let $mms_copy=0;

let $MYSQLD_DATADIR= `SELECT @@datadir`;
--disable_warnings
-- error 0,1
remove_file $MYSQLTEST_VARDIR/tmp/mms_for_$mms_purpose.aria_log_control;
--enable_warnings
copy_file $MYSQLD_DATADIR/$MARIA_LOG/aria_log_control $MYSQLTEST_VARDIR/tmp/mms_for_$mms_purpose.aria_log_control;

connection default;
