call mtr.add_suppression("Attempting backtrace. You can use the following information to find out");
call mtr.add_suppression("Assertion `0' failed");

--echo # Points to verify in this test
--echo # scenario-1. if crash happens just before deleting the schema directory, recovery will delete the directory.
--echo # scenario-2. if crash happens happens after dropping 1/n tables, recovery should undo the transaction and
--echo #             both dd entry and ddl log table should be restored in their original state.
--echo # scenario-3. if schema has tables from non transactional engines, DROP SCHEMA should behave transactional way. 

--source include/have_debug.inc

--echo # scenario-1

# crete the database
CREATE SCHEMA DB1;

# activate the debug point
SET DEBUG = "+d,MAKE_SERVER_ABORT_BEFORE_DELETING_THE_SCHEMA_DIR_NEW_WAY";

# this is supposed to crash
--source include/expect_crash.inc
--error 0,2002,2013
DROP SCHEMA DB1;
--source include/wait_until_disconnected.inc

# restart
--source include/start_mysqld.inc
--source include/wait_until_connected_again.inc

# deactivate the debug point
SET DEBUG = "-d,MAKE_SERVER_ABORT_BEFORE_DELETING_THE_SCHEMA_DIR_NEW_WAY";

# verify if the database really got deleted.
CREATE SCHEMA DB1;

--echo # scenario-2

SET DEBUG = "+d,MAKE_SERVER_ABORT_AFTER_DROPPING_ONE_TABLE";

CREATE TABLE DB1.T1 (C1 INT);
CREATE TABLE DB1.T2 (C1 INT);

# this is supposed to crash
--source include/expect_crash.inc
--error 0,2002,2013
DROP SCHEMA DB1;
--source include/wait_until_disconnected.inc

# restart
--source include/start_mysqld.inc
--source include/wait_until_connected_again.inc

SET DEBUG = "-d,MAKE_SERVER_ABORT_AFTER_DROPPING_ONE_TABLE";

--echo # scenario-3

# verify that db didn't get deleted previously
# also, final clean-up.
CREATE TABLE DB1.T3 (C1 INT) ENGINE=MyISAM;

# Make server crash just before deleting the directory
SET DEBUG = "+d,MAKE_SERVER_ABORT_BEFORE_DELETING_THE_SCHEMA_DIR_OLD_WAY";

# this is supposed to crash
--source include/expect_crash.inc
--error 0,2002,2013
DROP SCHEMA DB1;
--source include/wait_until_disconnected.inc

# restart
--source include/start_mysqld.inc
--source include/wait_until_connected_again.inc

SET DEBUG = "-d,MAKE_SERVER_ABORT_BEFORE_DELETING_THE_SCHEMA_DIR_OLD_WAY";

--error ER_SCHEMA_DIR_EXISTS,ER_BAD_DB_ERROR
CREATE SCHEMA DB1; 

# final cleanup 
let MYSQLD_DATADIR =`SELECT @@datadir`;
--force-rmdir $MYSQLD_DATADIR/DB1
