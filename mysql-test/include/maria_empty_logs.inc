# Maria help script.
# Cleans up all logs to give recovery a fresh start.

# API: set mel_keep_control_file=1 if want to keep control file;
# uses vardir, port and socket.

connection default;
let $default_db=`select database()`;

connection admin;
-- echo * shut down mysqld, removed logs, restarted it
append_file $MYSQLTEST_VARDIR/tmp/master0.expect;
wait-maria_empty_logs.inc
EOF

--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT  -u root --password= shutdown 2>&1;

if (!$mel_keep_control_file)
{
  remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log_control;
}
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000001;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000002;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000003;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000004;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000005;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000006;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000007;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000008;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000009;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000010;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000011;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000012;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000013;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000014;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000015;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000016;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000017;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000018;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000019;
-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/$MARIA_LOG/maria_log.00000020;
# hope there are not more than these logs...

-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/maria_recovery.trace;

append_file $MYSQLTEST_VARDIR/tmp/master0.expect;
restart-maria_empty_logs.inc
EOF

--source include/wait_until_connected_again.inc

connection default;
# Restore current database as the effect of "use" was lost after restart
--disable_query_log
eval use $default_db;
--enable_query_log
