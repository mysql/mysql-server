
# Clean all configuration changes after running the test to
# make sure the test is repeatable.
--source include/force_restart.inc

call mtr.add_suppression("You need to use --log-bin to make --binlog-format work.");
call mtr.add_suppression("You need to use --log-bin to make --log-replica-updates work.");

SELECT @@GLOBAL.log_bin;

--echo #
--echo # Verify that the log-replica-updates warning is printed when starting
--echo # the server and log-replica-updates is enabled as before, if both
--echo # --skip-log-bin and --log-replica-updates options are set explicitly
--echo # on command line or configuration file.
--echo #
--let $assert_file= $MYSQLTEST_VARDIR/log/mysqld.1.err
--let $assert_count= 1
--let $assert_select= You need to use --log-bin to make --log-replica-updates work.
--let $assert_text= Found the expected warning "You need to use --log-bin to make --log-replica-updates work." in server's error log.
--source include/assert_grep.inc

SELECT @@GLOBAL.log_replica_updates;
