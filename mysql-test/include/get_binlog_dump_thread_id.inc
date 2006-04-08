--exec $MYSQL test -e 'show processlist' | grep 'Binlog Dump' | cut -f1 > $MYSQLTEST_VARDIR/tmp/bl_dump_thread_id
--disable_warnings
drop table if exists t999;
--enable_warnings
create temporary table t999 (f int);
### --replace_result $MYSQL_TEST_DIR "."  #failed on solaris etc bug#18906
--disable_query_log
eval LOAD DATA INFILE "$MYSQLTEST_VARDIR/tmp/bl_dump_thread_id" into table t999;
--enable_query_log
let $id = `select f from t999`;
drop table t999;
