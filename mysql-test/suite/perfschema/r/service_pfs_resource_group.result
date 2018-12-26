
## Test the Performance Schema Resource Group service WL#8881
##
## 1. Load component to register callbacks for each API
## 2. Create users to trigger tests within the component
## 3. Write test results to a log file
## 4. Print out log file
##
## Note: The test component sets its own resource group name, PFS_CURRENT_THREAD, for root.

## Install test component
INSTALL COMPONENT 'file://component_test_pfs_resource_group';

## Create test-specific users. The internal tests are triggered when the user connects.
CREATE USER PFS_TEST_CURRENT_THREAD;
CREATE USER PFS_TEST_VALID_GROUP_NAME;
CREATE USER PFS_TEST_INVALID_THREAD_ID;
CREATE USER PFS_TEST_INVALID_GROUP_NAME;
GRANT ALL ON *.* TO PFS_TEST_CURRENT_THREAD;
GRANT ALL ON *.* TO PFS_TEST_VALID_GROUP_NAME;
GRANT ALL ON *.* TO PFS_TEST_INVALID_THREAD_ID;
GRANT ALL ON *.* TO PFS_TEST_INVALID_GROUP_NAME;

## TEST 1. CURRENT THREAD (INSTRUMENTED)
## PFS_CURRENT_THREAD is the group name for the component connection, set during component initialization.
## This also exercises the THREADS.RESOURCE_GROUP index.

SELECT name, type, processlist_user, processlist_host, processlist_db, resource_group
FROM performance_schema.threads WHERE resource_group = 'PFS_CURRENT_THREAD';
name	type	processlist_user	processlist_host	processlist_db	resource_group
thread/sql/one_connection	FOREGROUND	root	localhost	test	PFS_CURRENT_THREAD

## TEST 2. VALID GROUP NAME
## set_resource_group() with a valid group name
## CONNECT PFS_TEST_VALID_GROUP_NAME

## TEST 3. INVALID THREAD_ID
## Test set_resource_group() with an invalid thread_id
## CONNECT PFS_TEST_INVALID_THREAD_ID

## TEST 4. INVALID GROUP NAME
## Test set_resource_group() with an invalid group name
## CONNECT PFS_TEST_INVALID_GROUP_NAME
## Disconnect PFS_TEST_VALID_GROUP_NAME
## Disconnect PFS_TEST_INVALID_THREAD_ID
## Disconnect PFS_TEST_INVALID_GROUP_NAME

## Connect default

## PRINT LOG FILE
=========================================
=========================================

## Uninstall component

UNINSTALL COMPONENT 'file://component_test_pfs_resource_group';

## Clean up

DROP USER PFS_TEST_CURRENT_THREAD;
DROP USER PFS_TEST_VALID_GROUP_NAME;
DROP USER PFS_TEST_INVALID_THREAD_ID;
DROP USER PFS_TEST_INVALID_GROUP_NAME;
