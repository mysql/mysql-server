#
--source include/have_log_bin.inc
--source include/have_debug.inc

--enable_connect_log

# Clean gtid_executed so that test can execute after other tests
RESET BINARY LOGS AND GTIDS;

--echo #
--echo # BUG#21114768 -- INCORRECT BINLOG ORDER FOR CONCURRENT
--echo #                 CREATE TRIGGER AND DROP TRIGGER
--echo #
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT PRIMARY KEY);

CREATE TRIGGER trigger_1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
connect(con1,localhost,root,,);
SET DEBUG_SYNC='trigger_ddl_stmt_before_write_to_binlog SIGNAL drop_trigger_ready_to_write_to_binlog WAIT_FOR second_create_trigger_end';
--send DROP TRIGGER trigger_1
--connection default

SET @start_session_value= @@session.lock_wait_timeout;
SET @@session.lock_wait_timeout= 1;
SET DEBUG_SYNC='now WAIT_FOR drop_trigger_ready_to_write_to_binlog';

# We deliberately consider both cases -- success and failure
# with error code ER_LOCK_WAIT_TIMEOUT. First case takes place
# for the server without mdl locking for trigger name,
# the second one is for case when mdl lock for trigger name is acquired.
--error 0,ER_LOCK_WAIT_TIMEOUT
CREATE TRIGGER trigger_1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END;
SET DEBUG_SYNC='now SIGNAL second_create_trigger_end';

--connection con1
--echo # reaping execution status for DROP TRIGGER
reap;
--connection default
--disconnect con1

--echo # For a server without mdl locking for a trigger name
--echo # the statement SHOW BINLOG EVENTS would returned
--echo # output that look likes the following:
--echo # binlog.000001    567    Query    1    755    use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER trigger_1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END
--echo # binlog.000001    820    Query    1    1008    use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER trigger_1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END
--echo # binlog.000001    1008    Anonymous_Gtid    1    1073    SET @@SESSION.GTID_NEXT= 'ANONYMOUS'
--echo # binlog.000001    1073    Query    1    1169    use `test`; DROP TRIGGER trigger_1
--echo # That is, two statements CREATE TRIGGER trigger_t1 follows one
--echo # after another then DROP TRIGGER.
--source include/rpl/deprecated/show_binlog_events.inc

DROP TABLE t1,t2;

--echo # This test case tries to check that in case several connections contest for
--echo # mdl lock while trying to create a trigger with the same name then one of
--echo # connections acquires the mdl lock and creates a trigger successfully and
--echo # another one is waiting until mdl lock be released and after that failed since
--echo # trigger has been already created.

CREATE TABLE t1 (a INT);

connect (con1,localhost,root,,);
SET DEBUG_SYNC='create_trigger_has_acquired_mdl SIGNAL trigger_creation_cont WAIT_FOR second_create_trigger_wait_on_lock';

--send CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET @a := 1

connect (con2,localhost,root,,);

--let $con2_id= `SELECT CONNECTION_ID()`
SET DEBUG_SYNC='now WAIT_FOR trigger_creation_cont';

--send CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET @a := 1

--connection default

let $wait_condition=
  SELECT count(*) = 1 FROM INFORMATION_SCHEMA.PROCESSLIST
  WHERE state = "Waiting for table metadata lock" AND id = $con2_id;

--source include/wait_condition.inc

SET DEBUG_SYNC='now SIGNAL second_create_trigger_wait_on_lock';

--connection con2

--echo reaping the second CREATE TRIGGER t1_bi BEFORE INSERT
--error ER_TRG_ALREADY_EXISTS
--reap

--connection default

--disconnect con1
--disconnect con2

# Mask a value of the column Created to make test reproducible
--replace_column 6 #
SHOW TRIGGERS LIKE 't1';

DROP TABLE t1;

--echo # This test case checks that CREATE TRIGGERS suspends on MDL
--echo # in case DROP TRIGGER being processed in the same time from
--echo # another connection.

CREATE TABLE t1 (a INT);
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;

connect (con1,localhost,root,,);
SET lock_wait_timeout = 1;

connect (con2,localhost,root,,);

SET DEBUG_SYNC='drop_trigger_has_acquired_mdl SIGNAL drop_trigger_took_mdl WAIT_FOR drop_trigger_cont';

--send DROP TRIGGER t1_bi

--connection con1

SET DEBUG_SYNC='now WAIT_FOR drop_trigger_took_mdl';

--error ER_LOCK_WAIT_TIMEOUT
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;

SET DEBUG_SYNC='now SIGNAL drop_trigger_cont';

--connection con2

--echo reaping execution status for DROP TRIGGER
--reap

--echo # It's expected nothing in the output of SHOW TRIGGERS
SHOW TRIGGERS LIKE 't1';

--connection default
DROP TABLE t1;

--disconnect con1
--disconnect con2

--echo
--echo #########################################################################
--echo # Bug#30964944 - SHOW CREATE TRIGGER FAILS WHILE FLUSH TABLES WITH READ
--echo #                LOCK;
--echo #
--echo # Test case to verify that SHOW CREATE TRIGGER is allowed under FLUSH
--echo # TABLES WITH READ LOCK.
--echo #
--echo #########################################################################
--echo
--echo # This statement takes a global read lock.
FLUSH TABLES WITH READ LOCK;
--echo
--echo # SHOW CREATE TRIGGER should not fail.
--replace_column 2 # 4 # 5 # 6 # 7 #
SHOW CREATE TRIGGER sys.sys_config_insert_set_user;
--echo
--echo # Cleanup
UNLOCK TABLES;
--echo

--echo #
--echo # Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT INSENSITIVE NAMES.
--echo #                Even trigger names are case and accent insensitive. Test case to verify
--echo #                MDL locking with the case and accent insensitive trigger names.
--echo #

--enable_connect_log
SET NAMES utf8mb3;
CREATE TABLE t1 (f1 INT);

--echo # Case 1: Test case to verify MDL locking from concurrent SHOW CREATE TRIGGER
--echo #         and DROP TRIGGER operation with case and accent insensitive trigger
--echo #         name.

CREATE TRIGGER cafe BEFORE INSERT ON t1 FOR EACH ROW SET @sum= @sum + NEW.f1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_trigger SIGNAL locked WAIT_FOR continue';
--send SHOW CREATE TRIGGER CaFé;
--echo # At this point SHARED lock is acquired on the trigger.

CONNECT (con1, localhost, root);
SET DEBUG_SYNC='now WAIT_FOR locked';
--send DROP TRIGGER CAFE;
--echo # DROP TRIGGER requires EXCLUSIVE lock on the trigger. DROP is blocked
--echo # until SHARED lock on the trigger is released.

CONNECT (con2, localhost, root);
let $wait_condition= SELECT COUNT(*) > 0 FROM information_schema.processlist
                     WHERE info LIKE 'DROP TRIGGER%' AND
                     state LIKE 'Waiting for % metadata lock';
source include/wait_condition.inc;
SET DEBUG_SYNC='now SIGNAL continue';

--CONNECTION default
--replace_column 2 # 3 # 4 # 5 # 6 # 7 #
--REAP

--CONNECTION con1
--REAP

--echo # Case 2: Test case to verify MDL locking from concurrent DROP TRIGGER and
--echo #         SHOW CREATE TRIGGER operation with case and accent insensitive
--echo #         trigger name.

--CONNECTION default
CREATE TRIGGER cafe BEFORE INSERT ON t1 FOR EACH ROW SET @sum= @sum + NEW.f1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_trigger SIGNAL locked WAIT_FOR continue';
--send DROP TRIGGER CAFE;
--echo # At this point EXCLUSIVE lock is acquired on the trigger.

--CONNECTION con1
SET DEBUG_SYNC='now WAIT_FOR locked';
--send SHOW CREATE TRIGGER CaFé;
--echo # SHOW CREATE TRIGGER requires SHARED lock on the trigger. Statement is blocked
--echo # until EXCLUSIVE lock on the trigger is released.

--CONNECTION con2
let $wait_condition= SELECT COUNT(*) > 0 FROM information_schema.processlist
                     WHERE info LIKE 'SHOW CREATE TRIGGER%' AND
                     state LIKE 'Waiting for % metadata lock';
source include/wait_condition.inc;
SET DEBUG_SYNC='now SIGNAL continue';

--CONNECTION default
--REAP

--CONNECTION con1
--error ER_TRG_DOES_NOT_EXIST
--REAP

#Cleanup
--CONNECTION default
DISCONNECT con1;
DISCONNECT con2;
DROP TABLE t1;
SET NAMES default;
--disable_connect_log

# Restore original value for lock_wait_timeout
SET @@session.lock_wait_timeout= @start_session_value;
