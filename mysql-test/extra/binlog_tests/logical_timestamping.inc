--let $MYSQLD_DATADIR= `select @@datadir`
--let $events_file=$MYSQLTEST_VARDIR/tmp/events.sql
--let GREP_PRINT_NOT_VERBOSE=1

RESET MASTER;

# A first event in a new binlog has commit parent timestamp as zero
# and itself as one.
CREATE TABLE t1 (a int) ENGINE= innodb;

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 > $events_file
--let GREP_FILE=$events_file
--let GREP_PATTERN=last_committed=0	sequence_number=1
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

# A next transaction increments either counter

--let $binlog_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
INSERT INTO t1 SET a=1;

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 --start-position=$binlog_pos > $events_file
--let GREP_PATTERN=last_committed=1	sequence_number=2
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

# Transaction's last committed timestamp is computed at its last
# being executed query.
# Due to this two last logged transactions in the following sequence
# must have the same last comitted timestamp.

connect (one,localhost,root,,test);
connect (two,localhost,root,,test);
connect (three,localhost,root,,test);

--let $rpl_connection_name=one
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=1;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
# (transaction timestamp,last committed) (2,3)
BEGIN;
INSERT INTO t1 SET a=2;
COMMIT;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
INSERT INTO t1 SET a=1;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
--let $binlog_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
# (3,4)
BEGIN;
INSERT INTO t1 SET a=2;
COMMIT;

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 --start-position=$binlog_pos > $events_file
--let GREP_PATTERN=last_committed=3	sequence_number=4
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc


--let $rpl_connection_name=one
--source include/rpl_connection.inc
# (3,5)
--let $binlog_pos= query_get_value(SHOW MASTER STATUS, Position, 1)
COMMIT;
--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 --start-position=$binlog_pos > $events_file
--let GREP_PATTERN=last_committed=3	sequence_number=5
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

# Two independent and concurrent (autoincrement)  transaction will either
# have the same commit parent as the last committed of so far, or
# one of them will be such to another.

--let $binlog_pos= query_get_value(SHOW MASTER STATUS, Position, 1)

--let $rpl_connection_name=one
--source include/rpl_connection.inc
--send INSERT INTO t1 SET a=1

--let $rpl_connection_name=two
--source include/rpl_connection.inc
--send INSERT INTO t1 SET a=2

--let $rpl_connection_name=one
--source include/rpl_connection.inc
--reap
--let $rpl_connection_name=two
--source include/rpl_connection.inc
--reap

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 --start-position=$binlog_pos > $events_file
--let GREP_PATTERN=last_committed=[56]	sequence_number=[67]

--echo Must be 2 occurences found
--source extra/rpl_tests/grep_pattern.inc


#
# Testing logging of transaction that commits after binlog rotation.
# The last committed of "rotated" transaction
# must be set to the uninitialized (0) value.
#

--let $rpl_connection_name=one
--source include/rpl_connection.inc
RESET MASTER;
INSERT INTO t1 SET a=1;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=2;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=3;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
COMMIT;

# Not "rotated" 2nd transaction is logged following the regular rule.
# Its timestamp pair of (1,2) must be found.
--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 > $events_file
--let GREP_PATTERN=last_committed=1	sequence_number=2
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

FLUSH LOGS;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
COMMIT;

# Now the proof: the "rotated" transaction is logged with uninitialized last committed
# as expected. Its timestamp pair of (0,1) must be found.

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000002 > $events_file
--let GREP_PATTERN=last_committed=0	sequence_number=1
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

#
# Testing logging of transaction that commits after RESET MASTER.
# The last committed of "rotated" transactions
# must be set to the uninitialized (0) value.
#

--let $rpl_connection_name=one
--source include/rpl_connection.inc
RESET MASTER;
INSERT INTO t1 SET a=1;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=2;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=3;

--let $rpl_connection_name=three
--source include/rpl_connection.inc
BEGIN;
INSERT INTO t1 SET a=4;

--let $rpl_connection_name=two
--source include/rpl_connection.inc
COMMIT;

# Not "rotated" 2nd transaction is logged following the regular rule.
# Its timestamp pair of (1,2) must be found.
--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 > $events_file
--let GREP_PATTERN=last_committed=1	sequence_number=2
--echo Must be 1 occurence found
--source extra/rpl_tests/grep_pattern.inc

RESET MASTER;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
COMMIT;
--let $rpl_connection_name=three
--source include/rpl_connection.inc
COMMIT;

# Now the proof: the "rotated" transactions are logged with uninitialized last committed
# as expected. Its timestamp pair of (0,[12]) must be found.

--exec $MYSQL_BINLOG --force-if-open $MYSQLD_DATADIR/master-bin.000001 > $events_file
--let GREP_PATTERN=last_committed=0	sequence_number=[12]
--echo Must be 2 occurences found
--source extra/rpl_tests/grep_pattern.inc

#
# Cleanup
#

DROP TABLE t1;
