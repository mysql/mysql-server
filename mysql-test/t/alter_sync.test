#
# RENAME-related tests which require debug sync.
#

--source include/have_debug_sync.inc

###########################################################################

--source include/count_sessions.inc

###########################################################################

--echo #
--echo # Bug#21966802: MAKE OPERATIONS ON DD TABLES IMMUNE TO KILL
--echo #

--enable_connect_log

--echo # Create a new connection.
connect (con1, localhost, root,,);

--echo # Create a table and Alter it, but stop executing right before the
--echo # object is stored persistently.
CREATE TABLE t1 (pk INTEGER PRIMARY KEY);
SET DEBUG_SYNC= 'before_storing_dd_object SIGNAL before_store WAIT_FOR cont';
--SEND ALTER TABLE t1 ADD fld1 INT, ALGORITHM=INPLACE

connection default;
--echo # From the default connection, get the thread id of the rename, and
--echo # kill the query.
SET DEBUG_SYNC= 'now WAIT_FOR before_store';
SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE INFO LIKE "ALTER TABLE%" INTO @thread_id;
KILL QUERY @thread_id;
SET DEBUG_SYNC= 'now SIGNAL cont';

connection con1;
--echo # Though KILL QUERY is executed when ALTER TABLE's THD is in the kill immune mode,
--echo # killed status for THD is set only while exiting from the kill immune mode.
--echo # Code checking THD::killed status outside kill immune mode reports an error.
--error ER_QUERY_INTERRUPTED
--reap

connection default;
--echo # Disconnect and cleanup.
DROP TABLE t1;
disconnect con1;
SET DEBUG_SYNC= 'RESET';
--disable_connect_log

--source include/wait_until_count_sessions.inc

###########################################################################
