################################################################################
# This test is only checking that capture code does not cause
# errors, for complete testing it will need to be completed when
# WL#6826, WL#6833, WL#6834, WL#6842, WL#6829, WL#6855 and WL#7237
# are finished.
#
# Test:
# 0. The test requires two servers: M1 and M2.
# 1. With both members ONLINE, create table t1 on M1, create table t2 on M2.
# 2. On M1, perform some DML operations, create temporary table t3.
#    On M2, perform DML operation.
# 3. On M1, check tables and GTID_EXECUTED details.
# 4. On M2, check tables and GTID_EXECUTED details.
#    Diff tables t1 and t2 on M1 and M2.
# 5. Clean up.
################################################################################
--let $group_replication_group_name= 8a94f357-aab4-11df-86ab-c80aa9429562
--source include/have_group_replication_plugin.inc
--source include/group_replication.inc

--connection server1
CREATE TABLE t1 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;

--connection server2
CREATE TABLE t2 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;

--source include/rpl/sync.inc

--connection server1
BEGIN;
INSERT INTO t1 VALUES (1);
INSERT INTO t2 VALUES (2);
COMMIT;

INSERT INTO t1 VALUES (3);

BEGIN;
INSERT INTO t1 VALUES (4);
ROLLBACK;

BEGIN;
INSERT INTO t1 VALUES (5);
INSERT INTO t1 VALUES (6);
COMMIT;

CREATE TEMPORARY TABLE t3 (c1 INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;

--connection server2
INSERT INTO t1 VALUES (1000);

--source include/rpl/sync.inc

--connection server1
--echo
--echo server1

--let $server1_gtid_executed= `SELECT @@GLOBAL.GTID_EXECUTED`
SHOW TABLES FROM test;

--connection server2
--echo
--echo server2
--let $server2_gtid_executed= `SELECT @@GLOBAL.GTID_EXECUTED`
SHOW TABLES FROM test;

--let $assert_text= server1 GTID_EXECUTED must be equal to server2 GTID_EXECUTED
--let $assert_cond= "$server1_gtid_executed" = "$server2_gtid_executed"
--source include/assert.inc

--let  $diff_tables= server1:test.t1, server2:test.t1
--source include/diff_tables.inc
--let  $diff_tables= server1:test.t2, server2:test.t2
--source include/diff_tables.inc
--echo

--connection server1
DROP TABLE t1, t2, t3;
--source include/group_replication_end.inc
