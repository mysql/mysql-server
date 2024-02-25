# To be used with partition mgm commands like
# ALTER TABLE t1 ADD PARTITION (LIST/RANGE PARTITIONING).
--echo # Crash testing ADD PARTITION
SET SESSION debug="+d,crash_add_partition_1";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_1";
SET SESSION debug="+d,crash_add_partition_2";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_2";
SET SESSION debug="+d,crash_add_partition_3";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_3";
SET SESSION debug="+d,crash_add_partition_4";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_4";
SET SESSION debug="+d,crash_add_partition_5";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_5";
SET SESSION debug="+d,crash_add_partition_6";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_6";
SET SESSION debug="+d,crash_add_partition_7";
--source suite/parts/inc/partition_crash.inc
SET SESSION debug="-d,crash_add_partition_7";
