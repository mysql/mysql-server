#
# RENAME-related tests which require debug sync.
#

--source include/have_debug_sync.inc

###########################################################################

--source include/count_sessions.inc

###########################################################################

--echo #
--echo # Bug#21442630: FAILING TO STORE AN UPDATED DD OBJECT DOES NOT REVERT
--echo #               IN-MEMORY CHANGES
--echo #

--enable_connect_log

--echo # Create a new connection.
connect (con1, localhost, root,,);

--echo # Create a table and rename it, but stop execution right before the
--echo # updated object is stored persistently.
CREATE TABLE t1 (pk INTEGER PRIMARY KEY);
SET DEBUG_SYNC= 'before_storing_dd_object SIGNAL before_store WAIT_FOR cont';
--SEND RENAME TABLE t1 TO t2;

connection default;
--echo # From the default connection, get the thread id of the rename, and
--echo # kill the query.
SET DEBUG_SYNC= 'now WAIT_FOR before_store';
SELECT ID FROM INFORMATION_SCHEMA.PROCESSLIST WHERE INFO LIKE "RENAME TABLE%" INTO @thread_id;
KILL QUERY @thread_id;
SET DEBUG_SYNC= 'now SIGNAL cont';

connection con1;
--echo # Reap the rename and try to drop the table being renamed. Without the patch,
--echo # the drop leads to an assert.
# Bug#21966802 makes DD operations immune to kill. So RENAME TABLE statement
# will not be interrupted in this case. So commenting following statement.
#--error ER_QUERY_INTERRUPTED
--reap
# DROP TABLE t1;
DROP TABLE t2;

connection default;
--echo # Disconnect and cleanup.
disconnect con1;
SET DEBUG_SYNC= 'RESET';
--disable_connect_log

###########################################################################

--source include/wait_until_count_sessions.inc

###########################################################################
