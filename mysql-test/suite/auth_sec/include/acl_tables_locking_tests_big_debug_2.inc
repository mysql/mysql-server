#
# Validate that FLUSH PRIVILEGES blocks concurrent ACL DDL
# for given isolation level
#
# Usage:
# --let $ISOLATION_LEVEL=<isolation level>
# --source ../include/acl_tables_locking_tests_big_debug_2.inc

--echo # -----------------------------------------------------------------------
--echo # Setup
--echo # Saving initial values
SET @global_start_value = @@global.transaction_isolation;
SELECT @global_start_value;
SET @session_start_value = @@session.transaction_isolation;
SELECT @session_start_value;

--echo # Setting transaction isolation to '$ISOLATION_LEVEL'
--eval SET GLOBAL transaction_isolation = '$ISOLATION_LEVEL'

--echo # -----------------------------------------------------------------------
--echo # Case 1: FLUSH PRIVILEGES + ACL DDLs

--echo # -------------------------------
--echo # Case 1.1: FLUSH PRIVILEGES + CREATE USER

--let $ATTEMPT_SQL=CREATE USER u1, u2
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.2: FLUSH PRIVILEGES + ALTER USER

--let $ATTEMPT_SQL=ALTER USER u1 IDENTIFIED BY 'abcd'
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.3: FLUSH PRIVILEGES + DROP USER

--let $ATTEMPT_SQL=DROP USER u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.4: FLUSH PRIVILEGES + RENAME USER

--let $ATTEMPT_SQL=RENAME USER u1 TO u2
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.5: FLUSH PRIVILEGES + CREATE ROLE

--let $ATTEMPT_SQL=CREATE ROLE r1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.6: FLUSH PRIVILEGES + DROP ROLE

--let $ATTEMPT_SQL=DROP ROLE r1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.7: FLUSH PRIVILEGES + GRANT privilege

--let $ATTEMPT_SQL=GRANT SELECT ON *.* TO u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.8: FLUSH PRIVILEGES + REVOKE privilege

--let $ATTEMPT_SQL=REVOKE SELECT ON *.* FROM u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.9: FLUSH PRIVILEGES + GRANT role

--let $ATTEMPT_SQL=GRANT r1 TO u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.10: FLUSH PRIVILEGES + REVOKE role

--let $ATTEMPT_SQL=REVOKE r1 FROM u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.11: FLUSH PRIVILEGES + SET PASSWORD

--let $ATTEMPT_SQL=SET PASSWORD FOR u1 = 'abcd'
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -------------------------------
--echo # Case 1.12: FLUSH PRIVILEGES + SET DEFAULT ROLE

--let $ATTEMPT_SQL=SET DEFAULT ROLE r1 to u1
--let $ATTEMPT_DEBUG_SYNC=wl14084_acl_ddl_before_mdl_acquisition SIGNAL flush_now WAIT_FOR proceed_with_acl_ddl
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=FLUSH PRIVILEGES
--let $EXECUTE_DEBUG=wl14084_trigger_acl_ddl_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR flush_now
--let $EXECUTE_DEBUG_SYNC_2=wl14084_flush_privileges_after_table_locks SIGNAL proceed_with_acl_ddl
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc

--echo # -----------------------------------------------------------------------
--echo # Cleanup
--echo # Restore initial values

--connection default
SET @@global.transaction_isolation = @global_start_value;
SELECT @@global.transaction_isolation;
SET @@session.transaction_isolation = @session_start_value;
SELECT @@session.transaction_isolation;
