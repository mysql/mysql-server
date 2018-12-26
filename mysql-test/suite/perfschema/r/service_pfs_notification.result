
## Test the Performance Schema Notification service WL#8881
##
## 1. Install component to register callbacks for each API
## 2. Launch multiple sessions to trigger callback events, log the events to file
## 3. Import log file into a table
## 4. Verify the contents of the table
## Disable resource groups for this test.
SET DEBUG='+d, disable_rg_pfs_notifications';

## Install test component
INSTALL COMPONENT "file://component_test_pfs_notification";

## Create 2 users to control the test mode, and 3 users for testing
CREATE USER PFS_MTR_MODE_ENABLE;
CREATE USER PFS_MTR_MODE_DISABLE;
CREATE USER PFS_MTR_REGISTER_INTERNAL;
CREATE USER PFS_MTR_UNREGISTER_INTERNAL;
CREATE USER PFS_MTR_NEGATIVE_TEST_CASES;
CREATE USER PFS_USER1;
CREATE USER PFS_USER2;
CREATE USER PFS_USER3;
GRANT ALL ON *.* TO PFS_MTR_MODE_ENABLE;
GRANT ALL ON *.* TO PFS_MTR_MODE_DISABLE;
GRANT ALL ON *.* TO PFS_MTR_REGISTER_INTERNAL;
GRANT ALL ON *.* TO PFS_MTR_UNREGISTER_INTERNAL;
GRANT ALL ON *.* TO PFS_MTR_NEGATIVE_TEST_CASES;
GRANT ALL ON *.* TO PFS_USER1;
GRANT ALL ON *.* TO PFS_USER2;
GRANT ALL ON *.* TO PFS_USER3;

## Enable MTR mode by connecting with a pre-defined username.


## Test internal registration


## Test negative test cases


## Test unregister internal


## Clean up special test cases
## Disconnect internal registration user
## Disconnect internal unregistration user
## Disconnect negative test case user

## Start 2 sessions to generate thread_create and session_connect events.
## The session_connect callback assigns a resource group name for each user.

## Session 1

## Verify PFS_USER1 resource group in PERFORMANCE_SCHEMA.THREADS
SELECT name, type, processlist_user, processlist_host, processlist_db, resource_group
FROM performance_schema.threads
WHERE resource_group = 'RESOURCE_GROUP_3_2_1' AND processlist_user = 'PFS_USER1';
name	type	processlist_user	processlist_host	processlist_db	resource_group
thread/sql/one_connection	FOREGROUND	PFS_USER1	localhost	test	RESOURCE_GROUP_3_2_1

## Start session 2
connect (con2, localhost, PFS_USER2, , test)

## Verify PFS_USER2 in PERFORMANCE_SCHEMA.THREADS
SELECT name, type, processlist_user, processlist_host, processlist_db, resource_group
FROM performance_schema.threads
WHERE resource_group = 'RESOURCE_GROUP_3_2_1' AND processlist_user = 'PFS_USER2';
name	type	processlist_user	processlist_host	processlist_db	resource_group
thread/sql/one_connection	FOREGROUND	PFS_USER2	localhost	test	RESOURCE_GROUP_3_2_1

## Change user from PFS_USER2 to PFS_USER3

## Disconnect 2 sessions to generate session_disconnect and thread_destroy events.

## Disconnect session 1
## Disconnect session 2
## Disconnect primary control session

## Disable MTR mode and activate RQG mode by connecting with a pre-defined username.

## Verify mode change

SELECT name, type, processlist_user, processlist_host, processlist_db, resource_group
FROM performance_schema.threads WHERE processlist_user = 'PFS_MTR_MODE_DISABLE';
name	type	processlist_user	processlist_host	processlist_db	resource_group
thread/sql/one_connection	FOREGROUND	PFS_MTR_MODE_DISABLE	localhost	test	RESOURCE_GROUP_1
## Disconnect second control session

## Uninstall component

UNINSTALL COMPONENT 'file://component_test_pfs_notification';

## Create results table

CREATE TABLE test.callback_events (callback TEXT, handle INT, ret_code INT,
thread_id BIGINT UNSIGNED, plist_id BIGINT UNSIGNED, os_thread BIGINT UNSIGNED,
resource_group TEXT, user TEXT, host TEXT, vcpu TEXT, priority TEXT);

## Import log file

LOAD DATA LOCAL INFILE 'MYSQLD_DATADIR/test_pfs_notification.log' INTO TABLE test.callback_events
FIELDS TERMINATED BY ' ' LINES TERMINATED BY '\n' STARTING BY '*** '
(@label, callback, @label, handle, @label, ret_code, @label, thread_id, @label, plist_id,
@label, os_thread, @label, resource_group, @label, user, @label, host, @label, vcpu, @label, priority);

## Verify results

SELECT * FROM test.callback_events WHERE callback = "session_change_user";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
session_change_user                	 ... 	0         	   ...    	   ...    	   ...    	RESOURCE_GROUP_3_2_1	PFS_USER3                     	localhost 

SELECT * FROM test.callback_events WHERE callback = "session_connect;
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
session_connect                    	 ... 	0         	   ...    	   ...    	   ...    	RESOURCE_GROUP_3_2_1	PFS_USER1                     	localhost 

SELECT * FROM test.callback_events WHERE callback = "session_disconnect";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
session_disconnect                 	 ... 	0         	   ...    	   ...    	   ...    	RESOURCE_GROUP_3_2_1	PFS_USER1                     	localhost 

SELECT * FROM test.callback_events WHERE callback = "thread_create";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      

SELECT * FROM test.callback_events WHERE callback = "thread_destroy";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
thread_destroy                     	 ... 	0         	   ...    	   ...    	   ...    	RESOURCE_GROUP_3_2_1	PFS_USER1                     	localhost 

SELECT * FROM test.callback_events WHERE callback = "register_notification_internal";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
register_notification_internal     	3         	   ...    	   ...    	   ...    	   ...    	                    	PFS_MTR_REGISTER_INTERNAL     	localhost 

SELECT * FROM test.callback_events WHERE callback LIKE "unregister_notification_internal";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      
unregister_notification_internal   	4         	0         	   ...    	   ...    	   ...    	                    	PFS_MTR_UNREGISTER_INTERNAL   	localhost 

SELECT * FROM test.callback_events WHERE callback = "register_notification(bad_cb)";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      

SELECT * FROM test.callback_events WHERE callback = "register_notification(nullptr)";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      

SELECT * FROM test.callback_events WHERE callback = "unregister_notification(bad_handle)";
callback                           	handle    	ret_code  	thread_id 	plist_id  	os_thread 	resource_group      	user                          	host      

## Clean up

DROP USER PFS_MTR_MODE_ENABLE;
DROP USER PFS_MTR_MODE_DISABLE;
DROP USER PFS_MTR_REGISTER_INTERNAL;
DROP USER PFS_MTR_UNREGISTER_INTERNAL;
DROP USER PFS_MTR_NEGATIVE_TEST_CASES;
DROP USER PFS_USER1;
DROP USER PFS_USER2;
DROP USER PFS_USER3;
DROP TABLE test.callback_events;
SET DEBUG='-d,disable_rg_pfs_notifications';
