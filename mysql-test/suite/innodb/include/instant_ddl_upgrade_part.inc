##########################################################################
# Test script to test UPGRADE from older version prtitioned table (not)
# having INSTANT ADD columns.
#
# $row_format is to be set to the row_format on which test is to be run.
##########################################################################

--echo # ------------------------------------------------------------
--echo # Start server with old datadir with table t1_$row_format having
--echo #     c1, c2, normal columns
--echo #     c3 INSTANT ADD column with default def_c3.
--echo #     c4 INSTANT ADD column with default def_c4.
--echo #     with 4 partitions (p0, p1, p2, p3)
--echo # ------------------------------------------------------------

eval SHOW CREATE TABLE t1_$row_format;

--echo # ------------------------------------------------------------
--echo # Read rows from upgraded table t1_$row_format
--echo # ------------------------------------------------------------
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # REORGANIZE PARTITION (p0 into p0_a p0_b)
--echo # ------------------------------------------------------------
eval SELECT * FROM t1_$row_format PARTITION (p0);

eval ALTER TABLE t1_$row_format ALGORITHM=INPLACE, REORGANIZE PARTITION p0 INTO (PARTITION p0_a VALUES LESS THAN (50), PARTITION p0_b VALUES LESS THAN (100));

eval SELECT * FROM t1_$row_format PARTITION (p0_a);
eval SELECT * FROM t1_$row_format PARTITION (p0_b);

eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # Insert a row in upgraded table t1_$row_format
--echo # ------------------------------------------------------------
eval INSERT INTO t1_$row_format values (60, "r7c2", "r7c3", "r7c4");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : IN PLACE for row with no version
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c2="temp" where c1=10;
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : IN PLACE for row with V1
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c2="temp" where c1=40;
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : NOT IN PLACE for row with no version
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c3="r1c3" where c1=10;
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # UPDATE : NOT IN PLACE for row with V1
--echo # ------------------------------------------------------------
eval UPDATE t1_$row_format SET c4="r2c4" where c1=40;
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT DROP c2
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format DROP COLUMN c2, ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values (50, "r8c3", "r8c4");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT ADD c5 at the end
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format ADD COLUMN c5 char(10) default "c5_def", ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values (150, "r9c3", "r9c4", "r9c5");
eval SELECT * FROM t1_$row_format ORDER BY c1;

--echo # ------------------------------------------------------------
--echo # INSTANT ADD c6 somewhere in between
--echo # ------------------------------------------------------------
eval ALTER TABLE t1_$row_format ADD COLUMN c6 char(10) default "c6_def" after c1, ALGORITHM=INSTANT;
--let $table_name=t1_$row_format
--source suite/innodb/include/print_instant_part_metadata.inc
eval SELECT * FROM t1_$row_format ORDER BY c1;
eval INSERT INTO t1_$row_format values (250, "r0c6", "r0c3", "r0c4", "r0c5");
eval SELECT * FROM t1_$row_format ORDER BY c1;

eval DROP TABLE t1_$row_format;

--echo # ------------------------------------------------------------
--echo # TRUNCATE Partition test for Upgraded table
--echo # ------------------------------------------------------------
--echo # table t2_$row_format having
--echo #     c1, c2, c3, c4 normal columns
--echo #     c5 INSTANT ADD column with default NULL.
--echo #     with 4 partitions (p0, p1, p2, p3)
--echo #
--echo #     Partition p0, p1, p2 are truncated.
--echo # ------------------------------------------------------------
eval SHOW CREATE TABLE t2_$row_format;

eval SELECT * FROM t2_$row_format ORDER BY c1;
--let $table_name=t2_$row_format
--source suite/innodb/include/print_instant_part_metadata.inc

eval INSERT INTO t2_$row_format values (101, 102, 103, "104", 105);
eval SELECT * FROM t2_$row_format ORDER BY c1;

eval Alter table t2_$row_format add column c6 INT DEFAULT 666 FIRST, algorithm=INSTANT;
eval SELECT * FROM t2_$row_format ORDER BY c1;
--source suite/innodb/include/print_instant_part_metadata.inc

eval INSERT INTO t2_$row_format values (206, 201, 202, 203, "204", 205);
eval SELECT * FROM t2_$row_format ORDER BY c1;

eval ALTER TABLE t2_$row_format DROP COLUMN c2, algorithm=INSTANT;
eval SELECT * FROM t2_$row_format ORDER BY c1;

eval ALTER TABLE t2_$row_format TRUNCATE partition p3;
# Row in partition p3 should have been removed but remaining 2 rows should be there.
eval SELECT * FROM t2_$row_format ORDER BY c1;
--source suite/innodb/include/print_instant_part_metadata.inc

eval DROP TABLE t2_$row_format;
