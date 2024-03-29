################################################################################
# inc/partition_alter3.inc                                                     #
#                                                                              #
# Purpose:                                                                     #
#   Tests for partition management commands for HASH and KEY partitioning      #
#                                                                              #
#------------------------------------------------------------------------------#
# Original Author: mleich                                                      #
# Original Date: 2006-04-11                                                    #
# Change Author:                                                               #
# Change Date:                                                                 #
# Change:                                                                      #
################################################################################

--echo
--echo #========================================================================
--echo #  1.    Partition management commands on HASH partitioned table
--echo #           column in partitioning function is of type DATE
--echo #========================================================================
# 1.   Create the table
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
eval CREATE TABLE t1 (f_date DATE, f_varchar VARCHAR(30));
# 2.   Fill the table t1 with records
INSERT INTO t1 (f_date, f_varchar)
SELECT CONCAT(CAST((f_int1 + 999) AS CHAR),'-02-10'), CAST(f_char1 AS CHAR)
FROM t0_template
WHERE f_int1 + 999 BETWEEN 1000 AND 9999;
# 3.   Calculate the number of inserted records.
SELECT IF(9999 - 1000 + 1 > @max_row, @max_row , 9999 - 1000 + 1)
       INTO @exp_row_count;
# DEBUG SELECT @exp_row_count;
# 4.   Print the layout, check Readability
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #------------------------------------------------------------------------
--echo #  1.1   Increase number of PARTITIONS
--echo #------------------------------------------------------------------------
--echo #  1.1.1 ADD PARTITION to not partitioned table --> must fail
--error ER_PARTITION_MGMT_ON_NONPARTITIONED
ALTER TABLE t1 ADD PARTITION (PARTITION part2);
#
--echo #  1.1.2 Assign HASH partitioning
ALTER TABLE t1 PARTITION BY HASH(YEAR(f_date));
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #  1.1.3 Assign other HASH partitioning to already partitioned table
--echo #        + test and switch back + test
ALTER TABLE t1 PARTITION BY HASH(DAYOFYEAR(f_date));
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
ALTER TABLE t1 PARTITION BY HASH(YEAR(f_date));
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #  1.1.4 Add PARTITIONS not fitting to HASH --> must fail
--error ER_PARTITION_WRONG_VALUES_ERROR
ALTER TABLE t1 ADD PARTITION (PARTITION part1 VALUES IN (0));
--error ER_PARTITION_WRONG_VALUES_ERROR
ALTER TABLE t1 ADD PARTITION (PARTITION part2 VALUES LESS THAN (0));
#
--echo #  1.1.5 Add two named partitions + test
ALTER TABLE t1 ADD PARTITION (PARTITION part1, PARTITION part7);
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #  1.1.6 Add two named partitions, name clash --> must fail
--error ER_SAME_NAME_PARTITION
ALTER TABLE t1 ADD PARTITION (PARTITION part1, PARTITION part7);
#
--echo #  1.1.7 Add one named partition + test
ALTER TABLE t1 ADD PARTITION (PARTITION part2);
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #  1.1.8 Add four not named partitions + test
ALTER TABLE t1 ADD PARTITION PARTITIONS 4;
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc

--echo #------------------------------------------------------------------------
--echo #  1.2   Decrease number of PARTITIONS
--echo #------------------------------------------------------------------------
--echo #  1.2.1 DROP PARTITION is not supported for HASH --> must fail
--error ER_ONLY_ON_RANGE_LIST_PARTITION
ALTER TABLE t1 DROP PARTITION part1;
#
--echo #  1.2.2 COALESCE PARTITION partitionname is not supported
--error ER_PARSE_ERROR
ALTER TABLE t1 COALESCE PARTITION part1;
#
--echo #  1.2.3 Decrease by 0 is non sense --> must fail
--error ER_COALESCE_PARTITION_NO_PARTITION
ALTER TABLE t1 COALESCE PARTITION 0;
#
--echo #  1.2.4 COALESCE one partition + test loop
let $loop= 7;
while ($loop)
{
   ALTER TABLE t1 COALESCE PARTITION 1;
   --source suite/parts/inc/partition_layout.inc
   --source suite/parts/inc/partition_check_read1.inc
   dec $loop;
}
--echo #  1.2.5 COALESCE of last partition --> must fail
--error ER_DROP_LAST_PARTITION
ALTER TABLE t1 COALESCE PARTITION 1;
#
--echo #  1.2.6 Remove partitioning
ALTER TABLE t1 REMOVE PARTITIONING;
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read1.inc
#
--echo #  1.2.7 Remove partitioning from not partitioned table --> ????
--error ER_PARTITION_MGMT_ON_NONPARTITIONED
ALTER TABLE t1 REMOVE PARTITIONING;
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

--echo
--echo #========================================================================
--echo #  2.    Partition management commands on KEY partitioned table
--echo #========================================================================
# 1.   Create the table
--disable_warnings
DROP TABLE IF EXISTS t1;
--enable_warnings
eval CREATE TABLE t1 (
$column_list
);
# 2.   Fill the table t1 with some records
eval $insert_all;
# 4.   Print the layout, check Readability
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc
#
--echo #------------------------------------------------------------------------
--echo #  2.1   Increase number of PARTITIONS
--echo #        Some negative testcases are omitted (already checked with HASH).
--echo #------------------------------------------------------------------------
--echo #  2.1.1 Assign KEY partitioning
ALTER TABLE t1 PARTITION BY KEY(f_int1);
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc
#
--echo #  2.1.2 Add PARTITIONS not fitting to KEY --> must fail
--error ER_PARTITION_WRONG_VALUES_ERROR
ALTER TABLE t1 ADD PARTITION (PARTITION part1 VALUES IN (0));
--error ER_PARTITION_WRONG_VALUES_ERROR
ALTER TABLE t1 ADD PARTITION (PARTITION part2 VALUES LESS THAN (0));
#
--echo #  2.1.3 Add two named partitions + test
ALTER TABLE t1 ADD PARTITION (PARTITION part1, PARTITION part7);
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc
#
--echo #  2.1.4 Add one named partition + test
ALTER TABLE t1 ADD PARTITION (PARTITION part2);
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc
#
--echo #  2.1.5 Add four not named partitions + test
ALTER TABLE t1 ADD PARTITION PARTITIONS 4;
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc

--echo #------------------------------------------------------------------------
--echo #  2.2   Decrease number of PARTITIONS
--echo #        Some negative testcases are omitted (already checked with HASH).
--echo #------------------------------------------------------------------------
--echo #  2.2.1 DROP PARTITION is not supported for KEY --> must fail
--error ER_ONLY_ON_RANGE_LIST_PARTITION
ALTER TABLE t1 DROP PARTITION part1;
#
--echo #  2.2.4 COALESCE one partition + test loop
let $loop= 7;
while ($loop)
{
   ALTER TABLE t1 COALESCE PARTITION 1;
   --source suite/parts/inc/partition_layout.inc
   --source suite/parts/inc/partition_check_read2.inc
   dec $loop;
}
--echo #  2.2.5 COALESCE of last partition --> must fail
--error ER_DROP_LAST_PARTITION
ALTER TABLE t1 COALESCE PARTITION 1;
#
--echo #  2.2.6 Remove partitioning
ALTER TABLE t1 REMOVE PARTITIONING;
--source suite/parts/inc/partition_layout.inc
--source suite/parts/inc/partition_check_read2.inc
#
--echo #  2.2.7 Remove partitioning from not partitioned table --> ????
--error ER_PARTITION_MGMT_ON_NONPARTITIONED
ALTER TABLE t1 REMOVE PARTITIONING;
DROP TABLE t1;
--source suite/parts/inc/partition_check_drop.inc

