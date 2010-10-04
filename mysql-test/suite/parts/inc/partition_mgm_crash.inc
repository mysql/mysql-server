# collection of tests which crashes the server and checks recovery.
# also using error injection to test recovery of failures.
# uses $DATADIR and $engine

--echo #
--echo # Bug#53676: Unexpected errors and possible table corruption on
--echo # ADD PARTITION and LOCK TABLE
--echo # Bug#53770: Server crash at handler.cc:2076 on LOAD DATA
--echo # after timed out COALESCE PARTITION
--echo # Extended crash recovery testing of fast_alter_partition_table.
call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");

let $create_statement= CREATE TABLE t1 (a INT, b VARCHAR(64))
ENGINE = $engine
PARTITION BY LIST (a)
(PARTITION p0 VALUES IN (0,1,2,3,4,5,6,7,8,9),
 PARTITION p10 VALUES IN (11,12,13,14,15,16,17,18,19));
let $insert_statement= INSERT INTO t1 VALUES (1, "Original from partition p0"), (2, "Original from partition p0"), (3, "Original from partition p0"), (4, "Original from partition p0"), (11, "Original from partition p1"), (12, "Original from partition p1"), (13, "Original from partition p1"), (14, "Original from partition p1");

let $crash_statement= ALTER TABLE t1 ADD PARTITION
(PARTITION p20 VALUES IN (20,21,22,23,24,25,26,27,28,29));
--source suite/parts/inc/partition_crash_add.inc
let $fail_statement= $crash_statement;
--source suite/parts/inc/partition_fail_add.inc
let $crash_statement= ALTER TABLE t1 DROP PARTITION p10;
--source suite/parts/inc/partition_crash_drop.inc
let $fail_statement= $crash_statement;
--source suite/parts/inc/partition_fail_drop.inc
let $crash_statement= ALTER TABLE t1 REORGANIZE PARTITION p10 INTO
(PARTITION p10 VALUES IN (10,11,12,13,14,15,16,17,18,19),
 PARTITION p20 VALUES IN (20,21,22,23,24,25,26,27,28,29));
--source suite/parts/inc/partition_crash_change.inc
let $fail_statement= $crash_statement;
--source suite/parts/inc/partition_fail_change.inc
