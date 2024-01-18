#
# [ Based on ./suite/rpl/t/rpl_flush_logs.test ]
# This test verifies if the 'flush individual logs' command
# works fine with mysqladmin.
#

--source include/rpl/init_source_replica.inc
--source include/have_binlog_format_row.inc

connection master;
#
# Test 'flush-logs error' command.
#
--echo # Test if mysqladmin supports 'flush-logs error' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs error

file_exists $MYSQLTEST_VARDIR/tmp/master_log.err;

--echo # Make sure binary logs were not flushed
--echo # after execute 'flush error logs' statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

--source include/rpl/sync_to_replica.inc
--echo # Make sure relay logs were not flushed
--echo # after execute 'flush error logs' statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000003;

connection master;
#
# Test 'flush-logs relay' command.
#
--echo # Test if mysqladmin supports 'flush-logs relay' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs relay

--source include/rpl/sync_to_replica.inc
--echo # Check if 'slave-relay-bin.000003' file is created
--echo # after executing 'flush-logs relay' command.
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000003;

connection master;
--echo # Make sure binary logs were not flushed
--echo # after executeing 'flush-logs relay' command.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

#
# Test 'flush-logs slow' command.
#
--echo # Test if mysqladmin supports 'flush-logs slow' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs slow

--echo # Make sure binary logs were not be flushed
--echo # after executing 'flush-logs slow' command.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

#
# Test 'flush-logs general' command.
#
--echo # Test if mysqladmin supports 'flush-logs general' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs general

--echo # Make sure binary logs were not flushed
--echo # after execute 'flush-logs general' command.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

#
# Test 'flush-logs engine' command.
#
--echo # Test if mysqladmin supports 'flush-logs engine' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs engine

--echo # Make sure binary logs were not flushed
--echo # after execute 'flush-logs engine' statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

#
# Test 'flush-logs binary' command.
#
--echo # Make sure the 'master-bin.000002' file does not
--echo # exist before execution of 'flush-logs binary' command.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;

--echo # Test if mysqladmin supports 'flush-logs binary' command.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs binary

--echo # Check if 'master-bin.000002' file is created
--echo # after execution of 'flush-logs binary' statement.
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000002;
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000001;

# Test 'flush error logs, relay logs' statement
--source include/rpl/sync_to_replica.inc
--echo # Make sure the 'slave-relay-bin.000006' file does not exist
--echo # exist before execute 'flush error logs, relay logs' statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000006;

connection master;

--echo # Test if mysqladmin support combining multiple kinds of logs into one statement.
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs error relay

file_exists $MYSQLTEST_VARDIR/tmp/master_log.err;

--echo # Make sure binary logs were not flushed
--echo # after execute 'flush error logs, relay logs' statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000003;

--source include/rpl/sync_to_replica.inc
--echo # Check the 'slave-relay-bin.000006' file is created after
--echo # execute 'flush error logs, relay logs' statement.
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000006;


#
# Test 'flush-logs' command
#
--echo # Make sure the 'slave-relay-bin.000007' and 'slave-relay-bin.000008'
--echo # files do not exist before execute 'flush error logs, relay logs'
--echo # statement.
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000007;
--error 1
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000008;

--source include/rpl/stop_applier.inc

connection master;

--echo # Test if 'flush-logs' command works fine and flush all the logs.

--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs

file_exists $MYSQLTEST_VARDIR/tmp/master_log.err;

--echo # Check 'master-bin.000003' is created
--echo # after executing 'flush-logs' command.
file_exists $MYSQLTEST_VARDIR/mysqld.1/data/master-bin.000003;

--source include/rpl/sync_to_replica_received.inc
--echo # Check the 'slave-relay-bin.000007' and 'slave-relay-bin.000008'
--echo # files are created after execute 'flush logs' statement.
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000007;
file_exists $MYSQLTEST_VARDIR/mysqld.2/data/slave-relay-bin.000008;

--source include/rpl/start_applier.inc
--source include/rpl/deinit.inc

--echo # Test multiple options to flush-logs

--echo # Must work
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs binary error ping

--echo # Must work
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs ping

--error 1
--exec $MYSQLADMIN --no-defaults -u root -S $MASTER_MYSOCK -P $MASTER_MYPORT flush-logs pong 2>&1
