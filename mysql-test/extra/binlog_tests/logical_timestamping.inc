RESET MASTER;

# A first event in a new binlog has commit parent timestamp as zero
# and itself as one.
CREATE TABLE t1 (a int) ENGINE= innodb;

--let $binlog_file= master-bin.000001

--let $logical_timestamps= 0 1
--source include/assert_logical_timestamps.inc

# A next transaction increments either counter

--let $binlog_position= query_get_value(SHOW MASTER STATUS, Position, 1)
INSERT INTO t1 SET a=1;

--let $logical_timestamps= 1 2
--source include/assert_logical_timestamps.inc

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
--let $binlog_position= query_get_value(SHOW MASTER STATUS, Position, 1)
# (3,4)
BEGIN;
INSERT INTO t1 SET a=2;
COMMIT;

--let $logical_timestamps= 3 4
--source include/assert_logical_timestamps.inc


--let $rpl_connection_name=one
--source include/rpl_connection.inc
# (3,5)
--let $binlog_position= query_get_value(SHOW MASTER STATUS, Position, 1)
COMMIT;
--let $logical_timestamps= 3 5
--source include/assert_logical_timestamps.inc

# Two independent and concurrent (autoincrement)  transaction will either
# have the same commit parent as the last committed of so far, or
# one of them will be such to another.

--let $binlog_position= query_get_value(SHOW MASTER STATUS, Position, 1)

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

--let $logical_timestamps= 5 6;[56] 7
--source include/assert_logical_timestamps.inc


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
--let $binlog_position=
--let $logical_timestamps= 0 1;1 2
--source include/assert_logical_timestamps.inc

FLUSH LOGS;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
COMMIT;

# Now the proof: the "rotated" transaction is logged with uninitialized last committed
# as expected. Its timestamp pair of (0,1) must be found.

--let $binlog_file= master-bin.000002
--let $logical_timestamps= 0 1
--source include/assert_logical_timestamps.inc

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
--let $binlog_file= master-bin.000001
--let $logical_timestamps= 0 1;1 2
--source include/assert_logical_timestamps.inc

RESET MASTER;

--let $rpl_connection_name=one
--source include/rpl_connection.inc
COMMIT;
--let $rpl_connection_name=three
--source include/rpl_connection.inc
COMMIT;

# Now the proof: the "rotated" transactions are logged with uninitialized last committed
# as expected. Its timestamp pair of (0,[12]) must be found.

--let $logical_timestamps= 0 1;0 2
--source include/assert_logical_timestamps.inc

#
# Cleanup
#

DROP TABLE t1;



#
# Testing DROP of multiple tables logging when a query produces few groups
# into binary log.
#

CREATE TABLE t1 (a int) ENGINE= innodb;
CREATE TABLE tm (a int) ENGINE= MyISAM;
RESET MASTER;
CREATE TEMPORARY TABLE ttm1 (a INT) ENGINE=MyISAM;
CREATE TEMPORARY TABLE tti1 (a INT) ENGINE=Innodb;

# The query is logged in three parts where only
# the first one is sequenced.
# Such logging is caused by just one cache is used for logging.

--echo *** The query is logged in three parts.

DROP TABLE tm,t1,ttm1,tti1;


--let $logical_timestamps= 0 1;1 2;2 3;3 4;4 5
--source include/assert_logical_timestamps.inc

#
# The final grep invocation should be done by the top level part.
# It may produce results sensitive to the test environment (e.g GTID).
#
