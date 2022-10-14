--echo # Resource groups that require DEBUG builds.

--echo # Test Resource Group Hints Feature
CREATE RESOURCE GROUP r1 TYPE=USER VCPU=0,1;
SET GLOBAL DEBUG='+d,pause_after_rg_switch';
--connect (con1,localhost,root,,)
--send SELECT /*+ RESOURCE_GROUP(r1) */ 1;
--connection default
SET DEBUG_SYNC= 'now WAIT_FOR execute_pfs_select';
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP='r1';
SET DEBUG_SYNC= 'now SIGNAL signal_to_continue';
--connection con1
reap;

--connection default
SET DEBUG_SYNC= 'now WAIT_FOR restore_finished';
--echo # There should be no thread associated with r1 once query execution is complete.
SELECT COUNT(*) = 1 FROM performance_schema.threads WHERE RESOURCE_GROUP='r1';
SET GLOBAL DEBUG='-d,pause_after_rg_switch';
--disconnect con1
DROP RESOURCE GROUP r1;


--echo # Restart with an Invalid Resource group.
SET SESSION debug= '+d,skip_dd_table_access_check';
if ($thr_prio_enable == 1)
{
# Insert invalid thread priority value along with invalid VCPU ranges
# and enable the resource group.
INSERT INTO mysql.resource_groups VALUES(4, 'r1', 'USER', 1, "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", -5, NULL);
}

if ($thr_prio_enable == 0)
{
# Insert invalid VCPU ids and enable the resource group.
INSERT INTO mysql.resource_groups VALUES(4, 'r1', 'USER', 1, "0100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", 0, NULL);
}
call mtr.add_suppression("Invalid cpu id.*");
call mtr.add_suppression("Validation of resource group.*");
call mtr.add_suppression("Invalid thread priority.*");
SET SESSION debug= '-d,skip_dd_table_access_check';
--echo "Restarting the server."
--source include/restart_mysqld.inc

--echo # The resource group should be in disabled along with warnings about invalid values.
SELECT * FROM INFORMATION_SCHEMA.RESOURCE_GROUPS WHERE RESOURCE_GROUP_NAME="r1";
DROP RESOURCE GROUP r1;
