# ==== Purpose ====
#
# Verify if the binlog contents reflect what is expected to be
# applied on a replica. Exports the sql statements in the binlog,
# swapping the database from test to verify and import once again,
# so that $table_name contents are selected.
#
# ==== Usage ====
#
# --let $table_name= <name of table>
# [--let $order_by= pk]
#

--let $include_filename= ndb_binlog_verify_table.inc
--source include/begin_include_file.inc

if (!$table_name)
{
  die Table name required;
}
if (!$order_by)
{
  --let $order_by= pk
}

--let $datadir=`select @@datadir`
--let $cmd = $MYSQL_BINLOG --disable-log-bin --rewrite-db="test->verify"
--let $cmd = $cmd --to-last-log --verbose $datadir/binlog.000001
#--echo $cmd
--let $file_name = $MYSQLTEST_VARDIR/tmp/compare_binlog.sql
--exec $cmd > $file_name

--disable_query_log
SET sql_log_bin=OFF;
CREATE DATABASE verify;
--enable_query_log

--exec $MYSQL < $file_name
--remove_file $file_name
eval SELECT * FROM verify.$table_name ORDER BY $order_by;

--disable_query_log
DROP DATABASE verify;
SET sql_log_bin=ON;
--enable_query_log

--let $table_name=
--let $order_by= 
--source include/end_include_file.inc
