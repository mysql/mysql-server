# To be used with partition mgm command
# ALTER TABLE t1 UPGRADE PARTITIONING
--echo # Test ALTER TABLE t1 UPGRADE PARTITIONING
SET SESSION debug="+d,crash_upgrade_partition_1";
--source suite/parts/inc/partition_crash_old_frm.inc
SET SESSION debug="-d,crash_upgrade_partition_1";
SET SESSION debug="+d,crash_upgrade_partition_2";
--source suite/parts/inc/partition_crash_old_frm.inc
SET SESSION debug="-d,crash_upgrade_partition_2";
SET SESSION debug="+d,crash_upgrade_partition_3";
--source suite/parts/inc/partition_crash_old_frm.inc
SET SESSION debug="-d,crash_upgrade_partition_3";
SET SESSION debug="+d,crash_upgrade_partition_4";
--source suite/parts/inc/partition_crash_old_frm.inc
SET SESSION debug="-d,crash_upgrade_partition_4";
SET SESSION debug="+d,crash_upgrade_partition_5";
--source suite/parts/inc/partition_crash_old_frm.inc
SET SESSION debug="-d,crash_upgrade_partition_5";
