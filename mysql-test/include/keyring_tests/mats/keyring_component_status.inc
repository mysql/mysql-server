# Tests related to performance_schema.keyring_component_status table

CALL mtr.add_suppression("No suitable 'keyring_component_metadata_query' service implementation found to fulfill the request");

--echo #
--echo # Bug#32390719: QUERYING KEYRING_COMPONENT_STATUS TABLE
--echo #               GENERATES AN ERROR IN THE SERVER LOG
--echo #

# Should show metadata obtained from keyring component
--replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
SELECT * FROM performance_schema.keyring_component_status;

# Should be empty
SELECT PRIO, ERROR_CODE, SUBSYSTEM, DATA FROM performance_schema.error_log WHERE ERROR_CODE='MY-013712';

--echo # Restarting server without keyring component
--source include/keyring_tests/helper/instance_backup_manifest.inc
let $restart_parameters = restart: $PLUGIN_DIR_OPT;
--source include/restart_mysqld_no_echo.inc

# Should be empty
SELECT * FROM performance_schema.keyring_component_status;

# Should show one row
SELECT PRIO, ERROR_CODE, SUBSYSTEM, DATA FROM performance_schema.error_log WHERE ERROR_CODE='MY-013712';

--source include/keyring_tests/helper/instance_restore_manifest.inc
