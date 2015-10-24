# To be used with partition mgm command
# ALTER TABLE t1 UPGRADE PARTITONING
--echo # Error recovery for ALTER TABLE t UPGRADE PARTITIONING
SET SESSION debug="+d,fail_upgrade_partition_1";
--source suite/parts/inc/partition_fail_old_frm.inc
SET SESSION debug="-d,fail_upgrade_partition_1";
SET SESSION debug="+d,fail_upgrade_partition_2";
--source suite/parts/inc/partition_fail_old_frm.inc
SET SESSION debug="-d,fail_upgrade_partition_2";
SET SESSION debug="+d,fail_upgrade_partition_3";
--source suite/parts/inc/partition_fail_old_frm.inc
SET SESSION debug="-d,fail_upgrade_partition_3";
SET SESSION debug="+d,fail_upgrade_partition_4";
--source suite/parts/inc/partition_fail_old_frm.inc
SET SESSION debug="-d,fail_upgrade_partition_4";
SET SESSION debug="+d,fail_upgrade_partition_5";
--source suite/parts/inc/partition_fail_old_frm.inc
SET SESSION debug="-d,fail_upgrade_partition_5";
