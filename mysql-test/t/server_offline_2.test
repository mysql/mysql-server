# Created: Horst Hunger 2014-05-19
# WL#3836:  Method to bring servers off line

-- source include/not_threadpool.inc

# test case 2.1.6 1)
--disable_warnings
SELECT * FROM performance_schema.global_variables WHERE variable_name LIKE '%offline_mode%';
--enable_warnings
# Save the global value to be used to restore the original value.
let $global_saved_tmp =  `SELECT @@global.offline_mode`;

--enable_connect_log
# This count may not be 1, because of the probably existing connections
# from the previous/parallel test runs
let $user_count= `SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE USER != 'event_scheduler'`;

# Create 3 non-super users

CREATE USER 'user1'@'localhost';
CREATE USER 'user2'@'localhost';
CREATE USER 'user3'@'localhost';
--connect(conu1,localhost,user1)
--connect(conu2,localhost,user2)
--connect(conu3,localhost,user3)

--connection default

SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

SET GLOBAL offline_mode = ON;

# Wait until all non super user have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc

SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# test case 2.1.1 5)
--disconnect default

# To remove connect info on client side (mysqltest client).
--disconnect conu1
--disable_connect_log
--disable_query_log
--error ER_SERVER_OFFLINE_MODE
--connect(conu1,localhost,user1)
--enable_query_log
--echo connect(conu1,localhost,user1)
--connect(root,localhost,root)
--echo connect(root,localhost,root)
# Wait until all non super user have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;

--echo connect(default,localhost,root)
--connect(default,localhost,root)
# test case 2.1.6 1)
SHOW VARIABLES LIKE '%offline_mode%';
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# Clean up

DROP USER 'user1'@'localhost';
DROP USER 'user2'@'localhost';
DROP USER 'user3'@'localhost';

--echo # Restoring the original values.
--eval SET @@global.offline_mode = $global_saved_tmp

