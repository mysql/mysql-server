# To be used with partition mgm commands like
# ALTER TABLE t1 COALESCE/REBUILD/REORGANIZE PARTITION.
--echo # Error recovery change partition (REORGANIZE/REBUILD/COALESCE
--echo # or ADD HASH PARTITION).
SET SESSION debug="+d,fail_change_partition_1";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_1";
SET SESSION debug="+d,fail_change_partition_2";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_2";
SET SESSION debug="+d,fail_change_partition_3";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_3";
SET SESSION debug="+d,fail_change_partition_4";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_4";
SET SESSION debug="+d,fail_change_partition_5";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_5";
SET SESSION debug="+d,fail_change_partition_6";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_change_partition_6";
