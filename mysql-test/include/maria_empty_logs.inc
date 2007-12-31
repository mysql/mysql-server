# Maria help script.
# Cleans up all logs to give recovery a fresh start.

# API: set mel_keep_control_file=1 if want to keep control file;
# uses vardir, port and socket.

connection admin;

-- echo * shut down mysqld, removed logs, restarted it
append_file $MYSQLTEST_VARDIR/tmp/master0.expect;
wait-maria_empty_logs.inc
EOF

--exec $MYSQLADMIN --no-defaults -S $MASTER_MYSOCK -P $MASTER_MYPORT  -u root --password= shutdown 2>&1;

if (!$mel_keep_control_file)
{
  remove_file $MYSQLTEST_VARDIR/master-data/maria_log_control;
}
remove_file $MYSQLTEST_VARDIR/master-data/maria_log.00000001;
-- error 0,1 # maybe there is just one log
remove_file $MYSQLTEST_VARDIR/master-data/maria_log.00000002;
# Hope there were not more than these logs.

-- error 0,1
remove_file $MYSQLTEST_VARDIR/master-data/maria_recovery.trace;

append_file $MYSQLTEST_VARDIR/tmp/master0.expect;
restart-maria_empty_logs.inc
EOF

--source include/wait_until_connected_again.inc

connection default;
# the effect of "use" is lost after a restart so we are back into db "test",
# because connection 'default' was created with db "test".
use mysqltest;
