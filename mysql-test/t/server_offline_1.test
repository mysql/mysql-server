# Created: Horst Hunger 2014-05-09
# WL#3836:  Method to bring servers off line

-- source include/not_threadpool.inc

# Save the global value to be used to restore the original value.
SET @global_saved_tmp =  @@global.offline_mode;

--enable_connect_log
# This count may not be 1, because of the probably existing connections
# from the previous/parallel test runs
let $user_count= `SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE USER != 'event_scheduler'`;
--eval SET @user_count= $user_count

# test case 2.1.1 1)
# Create 3 non-super users

CREATE USER 'user1'@'localhost';
CREATE USER 'user2'@'localhost';
CREATE USER 'user3'@'localhost';
--connect(conu1,localhost,user1)
--connect(conu2,localhost,user2)
--connect(conu3,localhost,user3)

--error ER_SPECIFIC_ACCESS_DENIED_ERROR
SET GLOBAL offline_mode = ON;

--connection default

SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

SET GLOBAL offline_mode = ON;

# Wait until all non super user have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc

SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# test case 2.1.1 6)
--echo error ER_SERVER_OFFLINE_MODE
--error ER_SERVER_OFFLINE_MODE
--connection conu1

--connection default
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';
SET GLOBAL offline_mode = OFF;

# test case 2.1.1 2)
# Create another super user session to see if it also survies.
--connect(conu4,localhost,root)
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# test case 2.1.1 6)
--connection default
SET GLOBAL offline_mode = OFF;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# Disconnect cleanup session infos on client side to be able to reconnect.
--disconnect conu1
--disconnect conu2
--disconnect conu3
--connect(conu1,localhost,user1)
--connect(conu2,localhost,user2)
--connect(conu3,localhost,user3)

--connection default
SELECT @user_count;
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

SET GLOBAL offline_mode = ON;

# Wait until all non super user have been disconnected (for slow machines)
let $count_sessions= 1 + $user_count;
--source include/wait_until_count_sessions.inc

SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;
SHOW STATUS LIKE 'threads_connected';

# Clean up
--disconnect conu1
--disconnect conu2
--disconnect conu3
--disconnect conu4

# Wait until all users have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc

DROP USER 'user1'@'localhost';
DROP USER 'user2'@'localhost';
DROP USER 'user3'@'localhost';

--echo # Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;
