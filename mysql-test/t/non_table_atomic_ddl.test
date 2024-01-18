###############################################################################
# Test file contains test cases to verify crash safe/atomic DDL operations on
#   - Stored routines
#   - Triggers
#   - Events
#   - Views
###############################################################################

--source include/have_debug.inc
--source include/have_log_bin.inc

# Save the initial number of concurrent sessions.
--source include/count_sessions.inc

# Clear previous tests binlog.
--disable_query_log
reset binary logs and gtids;
--enable_query_log

--echo #########################################################################
--echo # A. TEST CASES FOR DDL ON STORED ROUTINES.                             #
--echo #########################################################################

--echo #
--echo # Test case to verify if CREATE routine statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_create_routine_failure";
--error ER_SP_STORE_FAILED
CREATE PROCEDURE p() SELECT 1;
--error ER_SP_STORE_FAILED
CREATE FUNCTION f() RETURNS INT return 1;
SET SESSION DEBUG="-d,simulate_create_routine_failure";
--echo # Data-dictionary objects for p and f are not stored on
--echo # an error/a transaction rollback.
--echo # Following statements fails as p and f does not exists.
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE PROCEDURE p;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION f;
--echo # Binlog event for failed create routine statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Routines are created and binlog event is written on a transaction
--echo # commit.
CREATE PROCEDURE p() SELECT 1;
CREATE FUNCTION f() RETURNS INT return 1;
SHOW CREATE PROCEDURE p;
SHOW CREATE FUNCTION f;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if ALTER routine statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_alter_routine_failure";
--error ER_SP_CANT_ALTER
ALTER FUNCTION f comment "atomic DDL on routine";
--error ER_SP_CANT_ALTER
ALTER PROCEDURE p comment "atomic DDL on routine";
SET SESSION DEBUG="-d,simulate_alter_routine_failure";

SET SESSION DEBUG="+d,simulate_alter_routine_xcommit_failure";
--error ER_SP_CANT_ALTER
ALTER FUNCTION f comment "atomic DDL on routine";
--error ER_SP_CANT_ALTER
ALTER PROCEDURE p comment "atomic DDL on routine";
SET SESSION DEBUG="-d,simulate_alter_routine_xcommit_failure";

--echo # Comment for stored routines is not updated on an error/a transaction
--echo # rollback.
SHOW CREATE PROCEDURE p;
SHOW CREATE FUNCTION f;
--echo # Binlog event for failed ALTER routine statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Routines are altered and binlog event is written on a transaction commit.
ALTER FUNCTION f comment "atomic DDL on routine";
ALTER PROCEDURE p comment "atomic DDL on routine";
SHOW CREATE FUNCTION f;
SHOW CREATE PROCEDURE p;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if DROP routine statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_drop_routine_failure";
--error ER_SP_DROP_FAILED
DROP FUNCTION f;
--error ER_SP_DROP_FAILED
DROP PROCEDURE p;
SET SESSION DEBUG="-d,simulate_drop_routine_failure";
--echo # On an error/a transaction rollback, routines are not dropped. Following
--echo # statements pass in this case.
SHOW CREATE PROCEDURE p;
SHOW CREATE FUNCTION f;
--echo # Binlog event for failed drop routine statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Routines are dropped and binlog event is written on transaction commit.
DROP FUNCTION f;
DROP PROCEDURE p;
--source include/rpl/deprecated/show_binlog_events.inc
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE PROCEDURE p;
--error ER_SP_DOES_NOT_EXIST
SHOW CREATE FUNCTION f;

--echo #########################################################################
--echo # B. TEST CASES FOR DDL ON TRIGGERS.                                    #
--echo #########################################################################

# Clear previous tests binlog.
--disable_query_log
reset binary logs and gtids;
--enable_query_log

CREATE TABLE t1(a INT);
CREATE TRIGGER trig1 BEFORE INSERT ON t1 FOR EACH ROW BEGIN END;
--replace_column 6 # 7 # 8 # 9 # 10 # 11 #
SHOW TRIGGERS;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if CREATE trigger statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_create_trigger_failure";
--error ER_UNKNOWN_ERROR
CREATE TRIGGER trig2 AFTER INSERT ON t1 FOR EACH ROW BEGIN END;
SET SESSION DEBUG="-d,simulate_create_trigger_failure";
--echo # trig2 is not stored in the DD tables on an error/a transaction rollback.
--echo # Following statement does not lists trig2 trigger.
--replace_column 6 # 7 # 8 # 9 # 10 # 11 #
SHOW TRIGGERS;
--echo # Binlog for failed create trigger statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Trigger is created and binlog event is written on a transaction commit.
CREATE TRIGGER trig2 AFTER INSERT ON t1 FOR EACH ROW BEGIN END;
--replace_column 6 # 7 # 8 # 9 # 10 # 11 #
SHOW TRIGGERS;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if DROP trigger statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_drop_trigger_failure";
--error ER_UNKNOWN_ERROR
DROP TRIGGER trig2;
SET SESSION DEBUG="-d,simulate_drop_trigger_failure";
--echo # trig2 is not dropped from the DD tables on an error/a transaction
--echo # rollback.
--echo # Following statement lists trig2 trigger.
--replace_column 6 # 7 # 8 # 9 # 10 # 11 #
SHOW TRIGGERS;
--echo # Binlog for failed drop trigger statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Trigger is dropped and binlog event is written on a transaction commit.
DROP TRIGGER trig2;
--replace_column 6 # 7 # 8 # 9 # 10 # 11 #
SHOW TRIGGERS;
--source include/rpl/deprecated/show_binlog_events.inc

DROP TABLE t1;

--echo #########################################################################
--echo # C. TEST CASES FOR DDL ON EVENTS.                                      #
--echo #########################################################################

# Clear previous tests binlog.
--disable_query_log
reset binary logs and gtids;
--enable_query_log

--echo #
--echo # Test case to verify if CREATE EVENT statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_create_event_failure";
--error ER_UNKNOWN_ERROR
CREATE EVENT event1 ON SCHEDULE EVERY 1 YEAR DO SELECT 1;
SET SESSION DEBUG="-d,simulate_create_event_failure";
--echo # DD object of event is not stored on an error/a transaction rollback.
--echo # Following statement does not list any events.
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--echo # Binlog event for failed create event statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # DD object for event is stored and binlog is written in a transaction
--echo # commit cases.
CREATE EVENT event1 ON SCHEDULE EVERY 1 YEAR DO SELECT 1;
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Binlog event for "create event if not exists" statement is
--echo # created on a transaction commit.
CREATE EVENT IF NOT EXISTS event1 ON SCHEDULE EVERY 1 YEAR DO SELECT 1;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if ALTER EVENT statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_alter_event_failure";
--error ER_UNKNOWN_ERROR
ALTER EVENT event1 COMMENT "Atomic Event's DDL";
SET SESSION DEBUG="-d,simulate_alter_event_failure";
--echo # DD object of event is not altered on an error/a transaction rollback.
--echo # Hence changes are not reflected in the following statements.
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--echo # Binlog event for failed alter event statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Event is altered and binlog event is written on a transaction commit.
ALTER EVENT event1 COMMENT "Atomic Event's DDL";
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if DROP EVENT statement is atomic/crash safe.
--echo #
SET SESSION DEBUG="+d,simulate_drop_event_failure";
--error ER_UNKNOWN_ERROR
DROP EVENT event1;
--error ER_UNKNOWN_ERROR
DROP EVENT IF EXISTS event1;
SET SESSION DEBUG="-d,simulate_drop_event_failure";
--echo # On error/transaction rollback, event is not dropped. Following
--echo # statements list events.
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--echo # Binlog event for failed drop event statement is not created.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Event is dropped and binlog event is written on a transaction commit.
DROP EVENT event1;
DROP EVENT IF EXISTS event1;
--replace_column 3 # 4 # 9 # 10 # 13 # 14 # 15 #
SHOW EVENTS;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #########################################################################
--echo # D. TEST CASES FOR DDL ON VIEWS.                                       #
--echo #########################################################################

# Clear previous tests binlog.
--disable_query_log
reset binary logs and gtids;
--enable_query_log

CREATE VIEW v1 AS SELECT 1;
CREATE VIEW v2 AS SELECT * FROM v1;
DROP VIEW v1;
SHOW CREATE VIEW v2;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if CREATE VIEW statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_create_view_failure";
--error ER_UNKNOWN_ERROR
CREATE VIEW v1 AS SELECT 1;
SET SESSION DEBUG="-d,simulate_create_view_failure";
--echo # DD object for view v1 is not stored and binlog event for view v1
--echo # is not written in an error/a rollback scenarios.
--echo # View v1 is not listed in the following statement.
SHOW TABLES;
--echo # Following statement fails as view v1 does not exists.
--error ER_NO_SUCH_TABLE
SHOW CREATE VIEW v1;
--echo # View v2 state remains as invalid after transaction rollback.
SHOW CREATE VIEW v2;
--echo # Binlog for failed create view is not written.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # On transaction commit, DD object of view is stored and binlog
--echo # event for view v1 is written.
CREATE VIEW v1 AS SELECT 1;
SHOW CREATE VIEW v1;
--echo # Referencing view v2's status is updated.
SHOW CREATE VIEW v2;
--echo # View v1 is listed.
SHOW TABLES;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if ALTER VIEW statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_create_view_failure";
--error ER_UNKNOWN_ERROR
ALTER VIEW v1 AS SELECT 1 as a1, 2 AS a2;
SET SESSION DEBUG="-d,simulate_create_view_failure";
--echo # DD object for view v1 is not stored and binlog event for view v1
--echo # is not written in an error/a rollback scenarios.
--echo # No change in the view v1 definition and v2 state.
SHOW TABLES;
SHOW CREATE VIEW v1;
SHOW CREATE VIEW v2;
--echo # Binlog for failed alter view is not written.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # On a transaction commit, DD object of view is stored and binlog
--echo # event for view v1 is written.
ALTER VIEW v1 AS SELECT 1 as a1, 2 AS a2;
SHOW CREATE VIEW v1;
SHOW CREATE VIEW v2;
SHOW TABLES;
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to verify if DROP VIEW statement is atomic/crash safe.
--echo #

SET SESSION DEBUG="+d,simulate_drop_view_failure";
--error ER_UNKNOWN_ERROR
DROP VIEW v1;
SET SESSION DEBUG="-d,simulate_drop_view_failure";
--echo # DD object for view v1 is not dropped and binlog event for view v1
--echo # is not written in an error/ia rollback scenarios.
--echo # No change in the view v1 definition and v2 state.
SHOW CREATE VIEW v1;
SHOW CREATE VIEW v2;
SHOW TABLES;
--echo # Binlog for failed drop view is not written.
--source include/rpl/deprecated/show_binlog_events.inc

--echo # On a transaction commit, DD object of view is dropped and binlog
--echo # event for view v1 is written.
DROP VIEW v1;
SHOW CREATE VIEW v2;
SHOW TABLES;
--source include/rpl/deprecated/show_binlog_events.inc

--echo # v1 view does not exists so only binlog is written in this case.
DROP VIEW IF EXISTS v1;
--source include/rpl/deprecated/show_binlog_events.inc

CREATE VIEW v3 AS SELECT 1;
CREATE VIEW v4 AS SELECT 1;
CREATE TABLE t1(f1 int);
--error ER_WRONG_OBJECT
DROP VIEW t1, v3, v4;
--echo # No binlog event is written in statement failure because of table
--echo # usage in the statement.
--source include/rpl/deprecated/show_binlog_events.inc
--echo # Existing views v4 & v5 are dropped and binlog is written in this
--echo # case.
DROP VIEW IF EXISTS v3, v4, v5;
--source include/rpl/deprecated/show_binlog_events.inc

#Clean up
DROP VIEW v2;
DROP TABLE t1;


--echo #########################################################################
--echo #                                                                       #
--echo # E. These test cases are added in order to test error code path that is#
--echo #    not covered by existing test cases, focusing gcov test coverage.   #
--echo #                                                                       #
--echo #########################################################################

# Clear previous tests binlog.
--disable_query_log
reset binary logs and gtids;
--enable_query_log

SET @orig_lock_wait_timeout= @@global.lock_wait_timeout;
SET GLOBAL lock_wait_timeout= 1;

--echo #
--echo # Test case to cover failure of method to update view/stored function
--echo # referencing views column metadata and status.
--echo #
--enable_connect_log

CREATE TABLE t1(f1 INT);
CREATE FUNCTION f() RETURNS INT return 1;
CREATE VIEW v1 AS SELECT 2 as f2;
CREATE VIEW v2 AS SELECT f() as f1, v1.f2 FROM v1;
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Acquire metadata lock on view v2.
SET DEBUG_SYNC="rm_table_no_locks_before_delete_table SIGNAL drop_func WAIT_FOR go";
--send DROP TABLE IF EXISTS t1, v2;

connect (con1, localhost, root,,);
SET DEBUG_SYNC="now WAIT_FOR drop_func";
--echo # Drop function "f" fails as lock acquire on view "v2" referencing "f"
--echo # times out.
--error ER_LOCK_WAIT_TIMEOUT
DROP FUNCTION f;
--source include/rpl/deprecated/show_binlog_events.inc

--echo # Drop function "f" fails as lock acquire on view "v2" referencing "f"
--echo # times out.
--error ER_LOCK_WAIT_TIMEOUT
DROP VIEW v1;
--source include/rpl/deprecated/show_binlog_events.inc
SET DEBUG_SYNC="now SIGNAL go";

--connection default
--reap

--echo # Status of view "v2" is set to invalid in the drop view statement.
DROP VIEW v1;
--echo # Acquire metadata lock on stored function "f".
SET DEBUG_SYNC='after_acquiring_mdl_lock_on_routine SIGNAL drop_view WAIT_FOR go';
--send DROP FUNCTION f;

connection con1;
SET DEBUG_SYNC='now WAIT_FOR drop_view';
--echo # Create view "v1" fails as column metadata update and status of
--echo # referencing view "v2" fails because lock acquire timeout on "f".
--error ER_LOCK_WAIT_TIMEOUT
CREATE VIEW v1 AS SELECT 2 as f2;
SET DEBUG_SYNC="now SIGNAL go";

connection default;
--reap
CREATE VIEW v1 AS SELECT 2 as f2;

# connection default;
--echo # Acquire metadata lock on view "v1".
LOCK TABLES v1 WRITE;

connection con1;
--echo # Create function "f" fails as column metadata update and status of view
--echo # referencing view "v2" fails because lock acquire timeout on view "v1".
--error ER_LOCK_WAIT_TIMEOUT
CREATE FUNCTION f() RETURNS INT return 1;

# Cleanup
connection default;
SET DEBUG_SYNC='RESET';
UNLOCK TABLES;
disconnect con1;
DROP VIEW v1, v2;
SET GLOBAL lock_wait_timeout= @orig_lock_wait_timeout;
--disable_connect_log
--echo # Binlog should not contain any event from failed statements.
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to cover stored routine and view object acquire method
--echo # failure.
--echo #
CREATE FUNCTION f1() RETURNS INT return 1;

SET SESSION DEBUG='+d,fail_while_acquiring_dd_object';
--error ER_LOCK_WAIT_TIMEOUT
CREATE FUNCTION f1() RETURNS INT return 1;
--error ER_LOCK_WAIT_TIMEOUT
ALTER FUNCTION f1 COMMENT "wl9173";
--error ER_LOCK_WAIT_TIMEOUT
CREATE VIEW v1 AS SELECT 1;
--error ER_LOCK_WAIT_TIMEOUT
DROP VIEW v1;
SET SESSION DEBUG='-d,fail_while_acquiring_dd_object';
DROP FUNCTION f1;

SET SESSION DEBUG='+d,fail_while_acquiring_routine_schema_obj';
--error ER_LOCK_WAIT_TIMEOUT
CREATE FUNCTION f1() RETURNS INT return 1;
SET SESSION DEBUG='-d,fail_while_acquiring_routine_schema_obj';

CREATE VIEW v1 AS SELECT 2 as f2;
SET SESSION DEBUG='+d,fail_while_acquiring_view_obj';
--error ER_LOCK_WAIT_TIMEOUT
DROP VIEW v1;
SET SESSION DEBUG='-d,fail_while_acquiring_view_obj';
DROP VIEW v1;

--echo # Binlog should not contain any event from failed statements.
--source include/rpl/deprecated/show_binlog_events.inc

--echo #
--echo # Test case to cover store routine drop object method failure.
--echo #
CREATE FUNCTION f1() RETURNS INT return 1;
SET DEBUG='+d,fail_while_dropping_dd_object';
--error ER_LOCK_WAIT_TIMEOUT
DROP FUNCTION f1;
SET DEBUG='-d,fail_while_dropping_dd_object';
DROP FUNCTION f1;
--echo # Binlog should not contain any event from failed statements.
--source include/rpl/deprecated/show_binlog_events.inc

--echo #########################################################################

# Check that all connections opened by test cases in this file are really gone
# so execution of other tests won't be affected by their presence.
--source include/wait_until_count_sessions.inc
