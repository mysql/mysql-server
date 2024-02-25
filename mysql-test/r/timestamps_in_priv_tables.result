CREATE TABLE t1(i int, j int);
CREATE USER test_user;
CREATE PROCEDURE timestamp_test() SELECT 1;
##################
# TEST FOR GRANT #
##################
# 1. Testing of mysql.columns_priv and mysql.tables_priv tables.
GRANT SELECT(i, j) ON t1 TO test_user;
# 2. Testing of mysql.procs_priv table.
GRANT EXECUTE ON PROCEDURE timestamp_test TO test_user;
# 3. Testing of mysql.proxies_priv table.
GRANT PROXY ON root TO test_user;
include/assert.inc [User has correct timestamp for mysql.tables_priv table.]
include/assert.inc [User has correct timestamp for mysql.columns_priv table.]
include/assert.inc [User has correct timestamp for mysql.procs_priv table.]
include/assert.inc [User has correct timestamp for mysql.proxies_priv table.]
# Verify that Server allows creation of tables with same table definition as of
# the grant tables when table has some entries in it.
CREATE TABLE tables_bkp AS SELECT USER,TIMESTAMP FROM mysql.tables_priv;
CREATE TABLE columns_bkp AS SELECT USER,TIMESTAMP FROM mysql.columns_priv;
CREATE TABLE procs_bkp AS SELECT USER,TIMESTAMP FROM mysql.procs_priv;
CREATE TABLE proxies_bkp AS SELECT USER,TIMESTAMP FROM mysql.proxies_priv;
###################
# TEST FOR REVOKE #
###################
# 1. Testing of mysql.columns_priv and mysql.tables_priv tables.
REVOKE SELECT(i) ON t1 FROM test_user;
# 2. Testing of mysql.procs_priv table.
REVOKE EXECUTE ON PROCEDURE timestamp_test FROM test_user;
# 3. Testing of mysql.proxies_priv table.
REVOKE PROXY ON root FROM test_user;
include/assert.inc [User has correct timestamp for mysql.tables_priv table.]
#######################
# TEST WITH MYSQLDUMP #
#######################
CREATE DATABASE dump;
CREATE TABLE dump.tables_priv AS SELECT * FROM mysql.tables_priv;
TRUNCATE TABLE dump.tables_priv;
include/diff_tables.inc [mysql.tables_priv, dump.tables_priv]
DROP DATABASE dump;
DROP TABLE procs_bkp;
DROP TABLE columns_bkp;
DROP TABLE tables_bkp;
DROP TABLE proxies_bkp;
DROP PROCEDURE timestamp_test;
DROP USER test_user;
DROP TABLE t1;
