# Resource groups that require DEBUG builds.
# Test Resource Group Hints Feature
CREATE RESOURCE GROUP r1 TYPE=USER VCPU=0,1;
SET GLOBAL DEBUG='+d,pause_after_rg_switch';
SELECT /*+ RESOURCE_GROUP(r1) */ 1;;
SET DEBUG_SYNC= 'now WAIT_FOR execute_pfs_select';
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP='r1';
COUNT(*) = 1
1
SET DEBUG_SYNC= 'now SIGNAL signal_to_continue';
1
1
SET DEBUG_SYNC= 'now WAIT_FOR restore_finished';
# There should be no thread associated with r1 once query execution is complete.
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP='r1';
COUNT(*) = 1
0
SET GLOBAL DEBUG='-d,pause_after_rg_switch';
DROP RESOURCE GROUP r1;
# Restart with an Invalid Resource group.
SET SESSION debug= '+d,skip_dd_table_access_check';
INSERT INTO mysql.resource_groups VALUES(3, 'r1', 'USER', 1, "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0, NULL);
call mtr.add_suppression("Invalid cpu id.*");
call mtr.add_suppression("Validation of resource group.*");
call mtr.add_suppression("Invalid thread priority.*");
SET SESSION debug= '-d,skip_dd_table_access_check';
"Restarting the server."
# restart
# The resource group should be in disabled along with warnings about invalid values.
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="r1";
RESOURCE_GROUP_NAME	RESOURCE_GROUP_TYPE	RESOURCE_GROUP_ENABLED	VCPU_IDS	THREAD_PRIORITY
r1	USER	0	275	0
DROP RESOURCE GROUP r1;
