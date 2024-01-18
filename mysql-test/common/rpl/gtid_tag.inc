# ==== PURPOSE ====
#
# This test a common part of tests with GTID tags which verify execution of the
# SET GTID_NEXT=AUTOMATIC:<tag> command
#
# ==== IMPLEMENTATION ====
#
# T1.
#     Test steps:
#
#     1. On the source: Execute one transaction with specified UUID and
#        specified tag
#     2. On the source: Execute one transaction with automatic GTID
#     3. On the source: Execute one transaction with specified UUID, untagged
#     4. On the source: Execute last transaction with automatic UUID and
#        specified tag
#
# ==== REFERENCES ====
#
# WL#15294 Extending GTID with tags to identify group of transactions
#          Steps 2 and 3
#

--source include/rpl/connection_source.inc

--echo
--echo # 1. Execute one transaction with specified UUID and
--echo #    specified tag
--echo

SET GTID_NEXT= "11111111-1111-1111-1111-111111111111:aa:1";
CREATE TABLE t1 (a INT AUTO_INCREMENT PRIMARY KEY) engine=Innodb;

--echo
--echo # 2. On the source: Execute one transaction with automatic GTID
--echo

SET GTID_NEXT= "AUTOMATIC";
INSERT INTO t1 VALUES (1);

--echo
--echo # 3. On the source: Execute one transaction with specified UUID, untagged
--echo

SET GTID_NEXT= "11111111-1111-1111-1111-111111111111:1";
INSERT INTO t1 VALUES (2);

--echo
--echo # 3. On the source: Execute one transaction with automatic UUID and
--echo #    specified tag
--echo

SET GTID_NEXT= "AUTOMATIC:aa";
DROP TABLE t1;

--source include/rpl/sync_to_replica.inc
