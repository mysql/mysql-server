--source include/force_myisam_default.inc
--source include/have_myisam.inc

# Created: Horst Hunger 2014-05-19
# WL#3836:  Method to bring servers off line

--source include/not_threadpool.inc

# Save the global value to be used to restore the original value.
SET @global_saved_tmp =  @@global.offline_mode;
SET @global_autocommit =  @@global.autocommit;
SET @@global.autocommit= OFF;

--enable_connect_log
# This count may not be 1, because of the probably existing connections
# from the previous/parallel test runs
let $user_count= `SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE USER != 'event_scheduler'`;

# Create a non-super user
CREATE USER 'user2'@'localhost';

--connect(conu2,localhost,user2)
START TRANSACTION;
CREATE TABLE t1 (c1 int,c2 char(10));
INSERT INTO t1 VALUES (1,'aaaaaaaaaa');
COMMIT;
INSERT INTO t1 VALUES (2,'bbbbbbbbbb');

# Disconnect cleanup session infos on client side to be able to reconnect.
--disconnect conu2

--connect(conu2,localhost,user2)
SELECT * FROM t1 ORDER BY c1;
DROP TABLE t1;

--connection default
SET GLOBAL offline_mode = ON;
# Wait until all non super user have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc
SELECT COUNT(USER) FROM INFORMATION_SCHEMA.PROCESSLIST;

# Clean up
--disconnect conu2
# Wait until all users have been disconnected (for slow machines)
let $count_sessions= $user_count;
--source include/wait_until_count_sessions.inc

DROP USER 'user2'@'localhost';

--echo # Restoring the original values.
SET @@global.offline_mode = @global_saved_tmp;
SET @@global.autocommit= @global_autocommit;




