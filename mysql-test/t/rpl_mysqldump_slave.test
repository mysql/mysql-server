
--source include/rpl/init_source_replica.inc

--echo #
--echo # New --dump-replica, --apply-replica-statements functionality
--echo #

# There is a gap between when START REPLICA returns and when Source_Log_File and
# SOURCE_LOG_POS are set.  Ensure that we don't call SHOW REPLICA STATUS during
# that gap.
--sync_slave_with_master

connection master;
use test;

connection slave;

--source include/rpl/save_binlog_file_position.inc

# Execute mysqldump with --dump-replica
--replace_regex /SOURCE_LOG_POS=[0-9]+/SOURCE_LOG_POS=BINLOG_START/
--exec $MYSQL_DUMP_SLAVE --compact --dump-replica test

--echo # Bug #35665076: there should be no binlog events
--let $event_sequence= ()
--source include/rpl/assert_binlog_events.inc

# Execute mysqldump with --dump-replica and --apply-replica-statements 
--replace_regex /SOURCE_LOG_POS=[0-9]+/SOURCE_LOG_POS=BINLOG_START/
--exec $MYSQL_DUMP_SLAVE --compact --dump-replica --apply-replica-statements test

--replace_regex /SOURCE_LOG_POS=[0-9]+/SOURCE_LOG_POS=BINLOG_START/
--replace_result $MASTER_MYPORT SOURCE_MYPORT
# Execute mysqldump with --dump-replica ,--apply-replica-statements and --include-source-host-port
--exec $MYSQL_DUMP_SLAVE --compact --dump-replica --apply-replica-statements --include-source-host-port test

--source include/rpl/deinit.inc
