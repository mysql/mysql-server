# This file is used by auth_sec.acl_tables_locking_tests_big added as part
# of wl#14084. The test focuses on concurrency test with MDL locking on ACL
# tables.
#
# Check behavior of ACL DDLs when simple and locking SELECTs
# are run concurrently with it.
#
# Usage:
# --let $ISOLATION_LEVEL=<isolation level>
# --source ../include/acl_tables_locking_tests_big.inc

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
--echo # ACL DDLs + Blocking READs

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.user WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.db WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.default_roles WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.role_edges WHERE from_user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.global_grants WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.global_grants WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.password_history WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.tables_priv WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.columns_priv WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.procs_priv WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--let $EXPECT_ERROR=1
--let $EXPECTED_ERROR_NUMBER=ER_LOCK_WAIT_TIMEOUT
--let $SQL=SELECT COUNT(*) FROM mysql.proxies_priv WHERE user LIKE 'u1' FOR UPDATE;
--source acl_tables_locking_tests_helper.inc

--echo # -----------------------------------------------------------------------
--echo # Cleanup
--echo # Restore initial values

--connection default
SET @@global.transaction_isolation = @global_start_value;
SELECT @@global.transaction_isolation;
SET @@session.transaction_isolation = @session_start_value;
SELECT @@session.transaction_isolation;
