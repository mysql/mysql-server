#
# Validate that ACL DDLs will block concurrent FLUSH PRIVILEGES
# for given isolation level
#
# Usage:
# --let $ISOLATION_LEVEL=<isolation level>
# --source ../include/acl_tables_locking_tests_big_debug_1.inc

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
--echo # Case 1: Successful ACL DDLs + FLUSH PRIVILEGES

--echo # -------------------------------
--echo # Case 1.1: CREATE USER + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=CREATE USER u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.2: ALTER USER + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=ALTER USER u1 IDENTIFIED BY 'abcd'
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.3: RENAME USER + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=RENAME USER u1 TO u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u2;

--echo # -------------------------------
--echo # Case 1.4: CREATE ROLE + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=CREATE ROLE r1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;

--echo # -------------------------------
--echo # Case 1.5: GRANT privilege + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=GRANT SELECT ON *.* TO u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.6: GRANT role + FLUSH PRIVILEGES

CREATE USER u1;
CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=GRANT r1 TO u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER r1;
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.7: SET DEFAULT ROLE + FLUSH PRIVILEGES

CREATE USER u1;
CREATE ROLE r1;
GRANT r1 TO u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=SET DEFAULT ROLE r1 TO u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER r1;
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.8: SET PASSWORD + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=SET PASSWORD FOR u1 = 'efgh'
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.9: REVOKE role + FLUSH PRIVILEGES

CREATE USER u1;
CREATE ROLE r1;
GRANT r1 TO u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=REVOKE r1 FROM u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.10: REVOKE privilege + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=REVOKE SELECT ON *.* FROM u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 1.11: DROP ROLE + FLUSH PRIVILEGES

CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=DROP ROLE r1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 1.12: DROP USER + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=DROP USER u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_after_table_lock_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=0
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -----------------------------------------------------------------------
--echo # Case 2: Unsuccessful ACL DDLs + FLUSH PRIVILEGES

--connect(conn_attempt_flush_privileges, localhost, root,,,,)
SET @@lock_wait_timeout=1;
--connect(conn_execute_acl_ddl, localhost, root,,,,)

--echo # -------------------------------
--echo # Case 2.1: CREATE USER + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=CREATE USER u3, u2, u1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 2.2: ALTER USER + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=ALTER USER u2 IDENTIFIED BY 'abcd'
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.3: RENAME USER + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=RENAME USER u2 TO u3
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.4: DROP USER + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=DROP USER u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.5: SET PASSWORD + FLUSH PRIVILEGES

CREATE USER u1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=SET PASSWORD FOR u1 = 'abcd'
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout, wl14084_simulate_set_password_failure
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_PASSWORD_NO_MATCH
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP USER u1;

--echo # -------------------------------
--echo # Case 2.6: CREATE ROLE + FLUSH PRIVILEGES

CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=CREATE ROLE r3, r2, r1
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;

--echo # -------------------------------
--echo # Case 2.7: DROP ROLE + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=DROP ROLE r2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANNOT_USER
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.8: GRANT privilege + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=GRANT SELECT ON *.* TO u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_CANT_CREATE_USER_WITH_GRANT
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.9: GRANT role + FLUSH PRIVILEGES

CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=GRANT r1 TO u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_UNKNOWN_AUTHID
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;

--echo # -------------------------------
--echo # Case 2.10: REVOKE role + FLUSH PRIVILEGES

CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=REVOKE r1 FROM u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_UNKNOWN_AUTHID
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;

--echo # -------------------------------
--echo # Case 2.11: REVOKE privilege + FLUSH PRIVILEGES

--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=REVOKE SELECT ON *.* FROM u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_NONEXISTING_GRANT
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default

--echo # -------------------------------
--echo # Case 2.12: SET DEFAULT ROLE + FLUSH PRIVILEGES

CREATE ROLE r1;
--let $ATTEMPT_SQL=FLUSH PRIVILEGES
--let $ATTEMPT_DEBUG=wl14084_simulate_flush_privileges_timeout
--let $ATTEMPT_DEBUG_SYNC=wl14084_flush_privileges_before_table_locks SIGNAL proceed_with_acl_ddl WAIT_FOR flush_now
--let $ATTEMPT_EXPECTED_ERROR_NUM=ER_LOCK_WAIT_TIMEOUT
--let $EXECUTE_SQL=SET DEFAULT ROLE r1 TO u2
--let $EXECUTE_DEBUG=wl14084_acl_ddl_error_before_cache_reload_trigger_timeout
--let $EXECUTE_DEBUG_SYNC_1=now WAIT_FOR proceed_with_acl_ddl
--let $EXECUTE_DEBUG_SYNC_2=wl14084_after_table_locks SIGNAL flush_now
--let $EXECUTE_EXPECT_ERROR=1
--let $EXECUTE_EXPECTED_ERROR_NUM=ER_ROLE_NOT_GRANTED
--source acl_tables_locking_tests_big_debug_helper.inc
--connection default
DROP ROLE r1;

--disconnect conn_attempt_flush_privileges
--disconnect conn_execute_acl_ddl

--echo # -----------------------------------------------------------------------
--echo # Cleanup
--echo # Restore initial values

--connection default
SET @@global.transaction_isolation = @global_start_value;
SELECT @@global.transaction_isolation;
SET @@session.transaction_isolation = @session_start_value;
SELECT @@session.transaction_isolation;
