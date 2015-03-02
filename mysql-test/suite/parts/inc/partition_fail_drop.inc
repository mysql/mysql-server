# To be used with partition mgm commands like
# ALTER TABLE t1 DROP PARTITION.
--echo # Error recovery DROP PARTITION
SET SESSION debug="+d,fail_drop_partition_1";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_drop_partition_1";
SET SESSION debug="+d,fail_drop_partition_2";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_drop_partition_2";
SET SESSION debug="+d,fail_drop_partition_3";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_drop_partition_3";
SET SESSION debug="+d,fail_drop_partition_4";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_drop_partition_4";
SET SESSION debug="+d,fail_drop_partition_5";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug="-d,fail_drop_partition_5";
