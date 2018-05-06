#
# Bug #27157520: SET PERSIST FOR GTID_MODE AND ENFORCE_GTID_CONSISTENCY IS NOT WORKING PROPERLY
#
SELECT @@gtid_mode;
@@gtid_mode
OFF
SET GLOBAL gtid_mode=1, gtid_mode=2, enforce_gtid_consistency=ON, gtid_mode=3;
SELECT @@gtid_mode;
@@gtid_mode
ON
SET PERSIST enforce_gtid_consistency=ON;
# add some delay to stabilize the test so that the
#   resoltion for persisted vars is enough
SELECT SLEEP(2);
SLEEP(2)
0
# No error since gtid_mode is set after enforce_gtid_consistency
SET PERSIST gtid_mode=ON;
# should see 2 rows
SELECT * FROM performance_schema.persisted_variables WHERE
VARIABLE_NAME IN ('enforce_gtid_consistency', 'gtid_mode');
VARIABLE_NAME	VARIABLE_VALUE
enforce_gtid_consistency	ON
gtid_mode	ON
# test if the order is persisted through restart
# restart
# should see 2 rows
SELECT * FROM performance_schema.persisted_variables WHERE
VARIABLE_NAME IN ('enforce_gtid_consistency', 'gtid_mode');
VARIABLE_NAME	VARIABLE_VALUE
enforce_gtid_consistency	ON
gtid_mode	ON
# Cleanup
RESET PERSIST gtid_mode;
RESET PERSIST enforce_gtid_consistency;
# restart for a cleanup
# restart
