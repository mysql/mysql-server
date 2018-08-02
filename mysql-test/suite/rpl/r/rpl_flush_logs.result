include/master-slave.inc
Warnings:
Note	####	Sending passwords in plain text without SSL/TLS is extremely insecure.
Note	####	Storing MySQL user name or password information in the master info repository is not secure and is therefore not recommended. Please consider using the USER and PASSWORD connection options for START SLAVE; see the 'START SLAVE Syntax' in the MySQL Manual for more information.
[connection master]
# Test if support 'flush error logs' statement.
flush error logs;
# Make sure binary logs was not be flushed
# after execute 'flush error logs' statement.
include/sync_slave_sql_with_master.inc
# Make sure relay logs was not be flushed 
# after execute 'flush error logs' statement.
# Test if support 'flush relay logs' statement.
flush relay logs;
include/sync_slave_sql_with_master.inc
# Check the 'slave-relay-bin.000003' file is created
# after executed 'flush relay logs' statement.
# Make sure binary logs was not be flushed
# after execute 'flush relay logs' statement.
# Test if support 'flush slow logs' statement.
flush slow logs;
# Make sure binary logs was not be flushed
# after execute 'flush slow logs' statement.
# Test if support 'flush general logs' statement.
flush general logs;
# Make sure binary logs was not be flushed
# after execute 'flush general logs' statement.
# Test if support 'flush engine logs' statement.
flush engine logs;
# Make sure binary logs was not be flushed
# after execute 'flush engine logs' statement.
# Make sure the 'master-bin.000002' file does not
# exist before execute 'flush binary logs' statement.
# Test if support 'flush binary logs' statement.
flush binary logs;
# Check the 'master-bin.000002' file is created
# after executed 'flush binary logs' statement.
include/sync_slave_sql_with_master.inc
# Make sure the 'slave-relay-bin.000006' file does not exist
# exist before execute 'flush error logs, relay logs' statement.
# Test if support to combine all kinds of logs into one statement.
flush error logs, relay logs;
# Make sure binary logs was not be flushed
# after execute 'flush error logs, relay logs' statement.
include/sync_slave_sql_with_master.inc
# Check the 'slave-relay-bin.000006' file is created after
# execute 'flush error logs, relay logs' statement.
# Make sure the 'slave-relay-bin.000007' and 'slave-relay-bin.000008'
# files do not exist before execute 'flush error logs, relay logs'
# statement.
include/stop_slave_sql.inc
# Test if 'flush logs' statement works fine and flush all the logs.
flush logs;
# Check 'master-bin.000003' is created
# after execute 'flush logs' statement.
include/sync_slave_io_with_master.inc
# Check the 'slave-relay-bin.000007' and 'slave-relay-bin.000008'
# files are created after execute 'flush logs' statement.
FLUSH SLOW LOGS;
ERROR HY000: File not found (Errcode: ##)
FLUSH GENERAL LOGS;
ERROR HY000: File not found (Errcode: ##)
include/assert.inc [assert that the above events are not written to binlog]
include/show_binlog_events.inc
include/sync_slave_io_with_master.inc
include/start_slave_sql.inc
include/rpl_end.inc
