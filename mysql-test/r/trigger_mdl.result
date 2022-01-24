RESET MASTER;
#
# BUG#21114768 -- INCORRECT BINLOG ORDER FOR CONCURRENT
#                 CREATE TRIGGER AND DROP TRIGGER
#
CREATE TABLE t1 (a INT PRIMARY KEY);
CREATE TABLE t2 (a INT PRIMARY KEY);
CREATE TRIGGER trigger_1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
connect con1,localhost,root,,;
SET DEBUG_SYNC='trigger_ddl_stmt_before_write_to_binlog SIGNAL drop_trigger_ready_to_write_to_binlog WAIT_FOR second_create_trigger_end';
DROP TRIGGER trigger_1;
connection default;
SET @start_session_value= @@session.lock_wait_timeout;
SET @@session.lock_wait_timeout= 1;
SET DEBUG_SYNC='now WAIT_FOR drop_trigger_ready_to_write_to_binlog';
CREATE TRIGGER trigger_1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END;
SET DEBUG_SYNC='now SIGNAL second_create_trigger_end';
connection con1;
# reaping execution status for DROP TRIGGER
connection default;
disconnect con1;
# For a server without mdl locking for a trigger name
# the statement SHOW BINLOG EVENTS would returned
# output that look likes the following:
# binlog.000001    567    Query    1    755    use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER trigger_1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END
# binlog.000001    820    Query    1    1008    use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER trigger_1 BEFORE INSERT ON t2 FOR EACH ROW BEGIN END
# binlog.000001    1008    Anonymous_Gtid    1    1073    SET @@SESSION.GTID_NEXT= 'ANONYMOUS'
# binlog.000001    1073    Query    1    1169    use `test`; DROP TRIGGER trigger_1
# That is, two statements CREATE TRIGGER trigger_t1 follows one
# after another then DROP TRIGGER.
include/show_binlog_events.inc
connection default;
Log_name	Pos	Event_type	Server_id	End_log_pos	Info
binlog.000001	#	Query	#	#	use `test`; CREATE TABLE t1 (a INT PRIMARY KEY)
binlog.000001	#	Query	#	#	use `test`; CREATE TABLE t2 (a INT PRIMARY KEY)
binlog.000001	#	Query	#	#	use `test`; CREATE DEFINER=`root`@`localhost` TRIGGER trigger_1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END
binlog.000001	#	Query	#	#	use `test`; DROP TRIGGER trigger_1
connection default;
connection default;
DROP TABLE t1,t2;
# This test case tries to check that in case several connections contest for
# mdl lock while trying to create a trigger with the same name then one of
# connections acquires the mdl lock and creates a trigger successfully and
# another one is waiting until mdl lock be released and after that failed since
# trigger has been already created.
CREATE TABLE t1 (a INT);
connect  con1,localhost,root,,;
SET DEBUG_SYNC='create_trigger_has_acquired_mdl SIGNAL trigger_creation_cont WAIT_FOR second_create_trigger_wait_on_lock';
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET @a := 1;
connect  con2,localhost,root,,;
SET DEBUG_SYNC='now WAIT_FOR trigger_creation_cont';
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW SET @a := 1;
connection default;
SET DEBUG_SYNC='now SIGNAL second_create_trigger_wait_on_lock';
connection con2;
reaping the second CREATE TRIGGER t1_bi BEFORE INSERT
ERROR HY000: Trigger already exists
connection default;
disconnect con1;
disconnect con2;
SHOW TRIGGERS LIKE 't1';
Trigger	Event	Table	Statement	Timing	Created	sql_mode	Definer	character_set_client	collation_connection	Database Collation
t1_bi	INSERT	t1	SET @a := 1	BEFORE	#	ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION	root@localhost	utf8mb4	utf8mb4_0900_ai_ci	utf8mb4_0900_ai_ci
DROP TABLE t1;
# This test case checks that CREATE TRIGGERS suspends on MDL
# in case DROP TRIGGER being processed in the same time from
# another connection.
CREATE TABLE t1 (a INT);
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
connect  con1,localhost,root,,;
SET lock_wait_timeout = 1;
connect  con2,localhost,root,,;
SET DEBUG_SYNC='drop_trigger_has_acquired_mdl SIGNAL drop_trigger_took_mdl WAIT_FOR drop_trigger_cont';
DROP TRIGGER t1_bi;
connection con1;
SET DEBUG_SYNC='now WAIT_FOR drop_trigger_took_mdl';
CREATE TRIGGER t1_bi BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
ERROR HY000: Lock wait timeout exceeded; try restarting transaction
SET DEBUG_SYNC='now SIGNAL drop_trigger_cont';
connection con2;
reaping execution status for DROP TRIGGER
# It's expected nothing in the output of SHOW TRIGGERS
SHOW TRIGGERS LIKE 't1';
Trigger	Event	Table	Statement	Timing	Created	sql_mode	Definer	character_set_client	collation_connection	Database Collation
connection default;
DROP TABLE t1;
disconnect con1;
disconnect con2;

#########################################################################
# Bug#30964944 - SHOW CREATE TRIGGER FAILS WHILE FLUSH TABLES WITH READ
#                LOCK;
#
# Test case to verify that SHOW CREATE TRIGGER is allowed under FLUSH
# TABLES WITH READ LOCK.
#
#########################################################################

# This statement takes a global read lock.
FLUSH TABLES WITH READ LOCK;

# SHOW CREATE TRIGGER should not fail.
SHOW CREATE TRIGGER sys.sys_config_insert_set_user;
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
sys_config_insert_set_user	#	CREATE DEFINER=`mysql.sys`@`localhost` TRIGGER `sys_config_insert_set_user` BEFORE INSERT ON `sys_config` FOR EACH ROW BEGIN
    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN
        SET NEW.set_by = USER();
    END IF;
END	#	#	#	#

# Cleanup
UNLOCK TABLES;

#
# Bug#28122841 - CREATE EVENT/PROCEDURE/FUNCTION CRASHES WITH ACCENT INSENSITIVE NAMES.
#                Even trigger names are case and accent insensitive. Test case to verify
#                MDL locking with the case and accent insensitive trigger names.
#
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
CREATE TABLE t1 (f1 INT);
# Case 1: Test case to verify MDL locking from concurrent SHOW CREATE TRIGGER
#         and DROP TRIGGER operation with case and accent insensitive trigger
#         name.
CREATE TRIGGER cafe BEFORE INSERT ON t1 FOR EACH ROW SET @sum= @sum + NEW.f1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_trigger SIGNAL locked WAIT_FOR continue';
SHOW CREATE TRIGGER CaFé;;
# At this point SHARED lock is acquired on the trigger.
CONNECT  con1, localhost, root;
SET DEBUG_SYNC='now WAIT_FOR locked';
DROP TRIGGER CAFE;;
# DROP TRIGGER requires EXCLUSIVE lock on the trigger. DROP is blocked
# until SHARED lock on the trigger is released.
CONNECT  con2, localhost, root;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
Trigger	sql_mode	SQL Original Statement	character_set_client	collation_connection	Database Collation	Created
cafe	#	#	#	#	#	#
connection con1;
# Case 2: Test case to verify MDL locking from concurrent DROP TRIGGER and
#         SHOW CREATE TRIGGER operation with case and accent insensitive
#         trigger name.
connection default;
CREATE TRIGGER cafe BEFORE INSERT ON t1 FOR EACH ROW SET @sum= @sum + NEW.f1;
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_trigger SIGNAL locked WAIT_FOR continue';
DROP TRIGGER CAFE;;
# At this point EXCLUSIVE lock is acquired on the trigger.
connection con1;
SET DEBUG_SYNC='now WAIT_FOR locked';
SHOW CREATE TRIGGER CaFé;;
# SHOW CREATE TRIGGER requires SHARED lock on the trigger. Statement is blocked
# until EXCLUSIVE lock on the trigger is released.
connection con2;
SET DEBUG_SYNC='now SIGNAL continue';
connection default;
connection con1;
ERROR HY000: Trigger does not exist
connection default;
disconnect con1;
disconnect con2;
DROP TABLE t1;
SET NAMES default;
SET @@session.lock_wait_timeout= @start_session_value;
