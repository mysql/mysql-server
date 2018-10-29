Tests of synchronization of stored procedure execution.
SET DEBUG_SYNC= 'RESET';
#
# Bug #30977 Concurrent statement using stored function and
#            DROP FUNCTION breaks SBR
#
# A stored routine could change after dispatch_command()
# but before a MDL lock is taken. This must be noticed and the
# sp cache flushed so the correct version can be loaded.
#
# Connection default
CREATE FUNCTION f1() RETURNS INT RETURN 1;
# Get f1 cached
SELECT f1();
f1()
1
# Then start executing it again...
SET DEBUG_SYNC= 'before_execute_sql_command SIGNAL before WAIT_FOR changed';
# Sending:
SELECT f1();
# Connection 2
SET DEBUG_SYNC= 'now WAIT_FOR before';
# ... but before f1 is locked, change it.
DROP FUNCTION f1;
CREATE FUNCTION f1() RETURNS INT RETURN 2;
SET DEBUG_SYNC= 'now SIGNAL changed';
# Connection default
# We should now get '2' and not '1'.
# Reaping: SELECT f1()
f1()
2
DROP FUNCTION f1;
SET DEBUG_SYNC= 'RESET';
#
# Field translation items must be cleared in case of back-offs
# for queries that use Information Schema tables. Otherwise 
# memory allocated in fix_fields() for views may end up referring
# to freed memory.
#
DROP FUNCTION IF EXISTS f1;
# Connection default
CREATE FUNCTION f1() RETURNS INT RETURN 0;
# Connection con2
SET DEBUG_SYNC= 'after_wait_locked_pname SIGNAL locked WAIT_FOR issued';
# con2 will now have an x-lock on f1
# Sending:
ALTER FUNCTION f1 COMMENT 'comment';
# Connection default
SET DEBUG_SYNC= 'now WAIT_FOR locked';
# This query will block due to the x-lock on f1 and back-off
SHOW OPEN TABLES WHERE f1()=0;
# Connection con3
# Check that the IS query is blocked before releasing the x-lock
SET DEBUG_SYNC= 'now SIGNAL issued';
# Connection default
# Reaping: ALTER FUNCTION f1 COMMENT 'comment'
DROP FUNCTION f1;
SET DEBUG_SYNC= 'RESET';
#
# Bug #48246 assert in close_thread_table
#
CREATE TABLE t0 (b INTEGER);
CREATE TABLE t1 (a INTEGER);
CREATE FUNCTION f1(b INTEGER) RETURNS INTEGER RETURN 1;
CREATE PROCEDURE p1() SELECT COUNT(f1(a)) FROM t1, t0;
INSERT INTO t0 VALUES(1);
INSERT INTO t1 VALUES(1), (2);
# Connection 2
CALL p1();
COUNT(f1(a))
2
SET DEBUG_SYNC= 'after_open_table_mdl_shared SIGNAL locked_t1 WAIT_FOR go_for_t0';
# This call used to cause an assertion. MDL deadlock with upcoming
# LOCK TABLES statement will cause back-off and retry.
# A variable indicating if a prelocking list exists, used to be not
# reset properly causing an eventual assert.
# Sending:
CALL p1();
# Connection default
SET DEBUG_SYNC= 'now WAIT_FOR locked_t1';
# Issue LOCK TABLES statement which will enter in MDL deadlock
# with CALL statement and as result will cause it to perform
# back-off and retry.
SET DEBUG_SYNC= 'mdl_acquire_lock_wait SIGNAL go_for_t0';
LOCK TABLES t0 WRITE, t1 WRITE;
UNLOCK TABLES;
# Connection 2
# Reaping: CALL p1()
COUNT(f1(a))
2
# Connection default
DROP PROCEDURE p1;
DROP FUNCTION f1;
DROP TABLES t0, t1;
# Case 1: Test case to verify MDL locking from concurrent SELECT and
#         DROP FUNCTION operation with case & access insensitive routine
#         name.
CREATE FUNCTION mIxEdCaSe() RETURNS INT RETURN 1;
SET DEBUG_SYNC='after_shared_lock_pname SIGNAL locked WAIT_FOR continue';
SELECT mIxEdCaSé();
# At this point we have a shared lock on 'mIxEdCaSé'
CONNECT  con1, localhost, root;
SET DEBUG_SYNC='now WAIT_FOR locked';
DROP FUNCTION mixedcase;;
# Without fix the shared lock does not prevent us from dropping the SF
# since the case used in this statement is different. This statement
# takes exclusive metadata lock on 'mixedcase', not 'mIxEdCaSe'.
# Drop function mIxEdCaSe will be blocked properly.
CONNECT  con2, localhost, root;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
mIxEdCaSé()
1
connection con1;
# Case 2: Test case to verify MDL locking from concurrent SHOW FUNCTION
#         and DROP FUNCTION operation with case and accent insensitive
#         function name.
connection default;
CREATE FUNCTION mIxEdCaSe() RETURNS INT RETURN 1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_routine SIGNAL locked WAIT_FOR continue';
DROP FUNCTION mIxEdCaSé;;
# At this point we have a exclusive lock on 'mIxEdCaSé'
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
SHOW CREATE FUNCTION MiXéDcAsE;
# This statement request for shared lock and it is blocked till DROP
# statement releases lock.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
connection con1;
ERROR 42000: FUNCTION MiXéDcAsE does not exist
# Case 3: Test case to verify MDL locking from concurrent DDL operations
#         with case and accent insensitive function name.
connection default;
CREATE FUNCTION mIxEdCaSe() RETURNS INT RETURN 1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_routine SIGNAL locked WAIT_FOR continue';
DROP FUNCTION miXEDCAsé;
# At this point we have a exclusive lock on 'miXEDCAsé'
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
ALTER FUNCTION mixedcase COMMENT "comment_string";;
# This statement request for exclusive lock and it is blocked till DROP
# statement releases lock.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
connection con1;
ERROR 42000: FUNCTION test.mixedcase does not exist
# Case 4: Test case to verify MDL locking from concurrent SHOW PROCEDURE
#         and DROP PROCEDURE operation with case and accent insensitive
#         procedure name.
connection default;
CREATE PROCEDURE mIxEdCaSe() BEGIN END;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_routine SIGNAL locked WAIT_FOR continue';
DROP PROCEDURE mIxEdCaSé;;
# At this point we have a exclusive lock on 'mIxEdCaSé'
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
SHOW CREATE PROCEDURE MiXéDcAsE;
# This statement request for shared lock and it is blocked till DROP
# statement releases lock.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
connection con1;
ERROR 42000: PROCEDURE MiXéDcAsE does not exist
# Case 5: Test case to verify MDL locking from concurrent DDL operations
#         with case & accent insensitive procedure name.
connection default;
CREATE PROCEDURE mIxEdCaSe() BEGIN END;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_routine SIGNAL locked WAIT_FOR continue';
DROP PROCEDURE miXEDCAsé;
# At this point we have a exclusive lock on 'miXEDCAsé'
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
ALTER PROCEDURE mixedcase COMMENT "comment_string";;
# This statement request for exclusive lock and it is blocked till DROP
# statement releases lock.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
connection con1;
ERROR 42000: PROCEDURE test.mixedcase does not exist
connection default;
SET DEBUG_SYNC='RESET';
disconnect con1;
disconnect con2;
