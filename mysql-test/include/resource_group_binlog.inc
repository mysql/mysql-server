--source include/have_log_bin.inc

--echo # Test to ensure resource group statements are not binlogged.
RESET BINARY LOGS AND GTIDS;
CREATE RESOURCE GROUP u1 TYPE=USER VCPU=0-2;
ALTER RESOURCE GROUP u1 VCPU=3;
SET RESOURCE GROUP u1;
--sorted_result
SELECT RESOURCE_GROUP_NAME FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
DROP RESOURCE GROUP u1 FORCE;
--source include/rpl/deprecated/show_binlog_events.inc
--sorted_result
SELECT RESOURCE_GROUP_NAME FROM INFORMATION_SCHEMA.RESOURCE_GROUPS;
