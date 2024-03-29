# === Purpose ===
#
# This test verifies that correct timestamp is generated for GRANT system
# tables (mysql.*_priv tables).
#
# === References ===
#
# Bug #98495: Timestamp is not set to CURRENT_TIMESTAMP in mysql.tables_priv
# PS-7617: Timestamp is not set to CURRENT_TIMESTAMP in grant tables

# Test Setup.
CREATE TABLE t1(i int, j int);
CREATE USER test_user;
CREATE PROCEDURE timestamp_test() SELECT 1;

--echo ##################
--echo # TEST FOR GRANT #
--echo ##################

# Record the timestamp before the test.
--let $test_start_time = `SELECT UNIX_TIMESTAMP(CURRENT_TIMESTAMP)`

--echo # 1. Testing of mysql.columns_priv and mysql.tables_priv tables.
GRANT SELECT(i, j) ON t1 TO test_user;

--echo # 2. Testing of mysql.procs_priv table.
GRANT EXECUTE ON PROCEDURE timestamp_test TO test_user;

--echo # 3. Testing of mysql.proxies_priv table.
GRANT PROXY ON root TO test_user;

# Record the timestamp after the test.
--let $test_end_time = `SELECT UNIX_TIMESTAMP(CURRENT_TIMESTAMP)`

# Validation of timestamp column in mysql.tables_priv
--let $user_creation_time = `SELECT UNIX_TIMESTAMP(Timestamp) FROM mysql.tables_priv WHERE User = 'test_user'`
--let $assert_text = User has correct timestamp for mysql.tables_priv table.
--let $assert_cond = [SELECT $user_creation_time BETWEEN $test_start_time AND $test_end_time] = 1
--source include/assert.inc

# Validation of timestamp column in mysql.columns_priv
--let $user_creation_time = `SELECT UNIX_TIMESTAMP(Timestamp) FROM mysql.columns_priv WHERE User = 'test_user'`
--let $assert_text = User has correct timestamp for mysql.columns_priv table.
--let $assert_cond = [SELECT $user_creation_time BETWEEN $test_start_time AND $test_end_time] = 1
--source include/assert.inc

# Validation of timestamp column in mysql.procs_priv
--let $user_creation_time = `SELECT UNIX_TIMESTAMP(Timestamp) FROM mysql.procs_priv WHERE User = 'test_user'`
--let $assert_text = User has correct timestamp for mysql.procs_priv table.
--let $assert_cond = [SELECT $user_creation_time BETWEEN $test_start_time AND $test_end_time] = 1
--source include/assert.inc

# Validation of timestamp column in mysql.proxies_priv
--let $user_creation_time = `SELECT UNIX_TIMESTAMP(Timestamp) FROM mysql.proxies_priv WHERE User = 'test_user'`
--let $assert_text = User has correct timestamp for mysql.proxies_priv table.
--let $assert_cond = [SELECT $user_creation_time BETWEEN $test_start_time AND $test_end_time] = 1
--source include/assert.inc

--echo # Verify that Server allows creation of tables with same table definition as of
--echo # the grant tables when table has some entries in it.
CREATE TABLE tables_bkp AS SELECT USER,TIMESTAMP FROM mysql.tables_priv;
CREATE TABLE columns_bkp AS SELECT USER,TIMESTAMP FROM mysql.columns_priv;
CREATE TABLE procs_bkp AS SELECT USER,TIMESTAMP FROM mysql.procs_priv;
CREATE TABLE proxies_bkp AS SELECT USER,TIMESTAMP FROM mysql.proxies_priv;

--echo ###################
--echo # TEST FOR REVOKE #
--echo ###################
# Record the timestamp before the test.
--let $test_start_time = `SELECT UNIX_TIMESTAMP(CURRENT_TIMESTAMP)`

--echo # 1. Testing of mysql.columns_priv and mysql.tables_priv tables.
REVOKE SELECT(i) ON t1 FROM test_user;

--echo # 2. Testing of mysql.procs_priv table.
REVOKE EXECUTE ON PROCEDURE timestamp_test FROM test_user;

--echo # 3. Testing of mysql.proxies_priv table.
REVOKE PROXY ON root FROM test_user;

# Record the timestamp after the test.
--let $test_end_time = `SELECT UNIX_TIMESTAMP(CURRENT_TIMESTAMP)`

#
# We only validate for mysql.tables_priv as the entries are deleted in case of
# other REVOKE queries.
#

# Validation of timestamp column in mysql.tables_priv
--let $user_creation_time = `SELECT UNIX_TIMESTAMP(Timestamp) FROM mysql.tables_priv WHERE User = 'test_user'`
--let $assert_text = User has correct timestamp for mysql.tables_priv table.
--let $assert_cond = [SELECT $user_creation_time BETWEEN $test_start_time AND $test_end_time] = 1
--source include/assert.inc

--echo #######################
--echo # TEST WITH MYSQLDUMP #
--echo #######################
--let $mysqldumpfile = $MYSQLTEST_VARDIR/tmp/mysqldumpfile.sql
--exec $MYSQL_DUMP --compact --replace --skip-add-locks --skip-lock-tables --no-create-info --skip-triggers --set-gtid-purged=OFF mysql tables_priv --skip-tz-utc > $mysqldumpfile

# We use CTAS for creating a table with exact schema, as DD doesn't allow
# CREATE TABLE LIKE query on mysql db tables.
CREATE DATABASE dump;
CREATE TABLE dump.tables_priv AS SELECT * FROM mysql.tables_priv;
TRUNCATE TABLE dump.tables_priv;
--exec $MYSQL dump < $mysqldumpfile

--let $diff_tables= mysql.tables_priv, dump.tables_priv
--source include/diff_tables.inc


# Cleanup
--remove_file $mysqldumpfile
DROP DATABASE dump;
DROP TABLE procs_bkp;
DROP TABLE columns_bkp;
DROP TABLE tables_bkp;
DROP TABLE proxies_bkp;

DROP PROCEDURE timestamp_test;
DROP USER test_user;
DROP TABLE t1;
